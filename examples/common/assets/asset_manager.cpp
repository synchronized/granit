// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_manager.h"

#include <mutex>
#include <new>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

namespace granit::example::assets {
namespace {

struct request_key {
  std::type_index type{typeid(void)};
  std::string location;

  friend bool operator==(const request_key&, const request_key&) = default;
};

struct request_key_hash {
  std::size_t operator()(const request_key& key) const noexcept {
    return key.type.hash_code() ^ (std::hash<std::string>{}(key.location) << 1U);
  }
};

void fail(const std::shared_ptr<detail::asset_state>& state, asset_error error,
          std::string diagnostic) {
  std::scoped_lock lock{state->mutex};
  const auto status = state->status.load(std::memory_order_acquire);
  if (status == asset_status::ready || status == asset_status::failed)
    return;
  state->error = error;
  state->diagnostic = std::move(diagnostic);
  state->progress.stage = asset_stage::complete;
  state->status.store(asset_status::failed, std::memory_order_release);
}

} // namespace

struct asset_manager::implementation : std::enable_shared_from_this<implementation> {
  explicit implementation(tasks::task_system& task_system) : tasks{task_system} {}

  struct dependency_batch {
    std::mutex mutex;
    std::shared_ptr<std::vector<std::byte>> document;
    std::vector<asset_dependency_data> dependencies;
    std::size_t completed{};
    bool failed{};
  };

  void post_failure(const std::shared_ptr<detail::asset_state>& state, asset_error error,
                    std::string diagnostic) {
    static_cast<void>(
        tasks.main().post([state, error, diagnostic = std::move(diagnostic)]() mutable {
          fail(state, error, std::move(diagnostic));
        }));
  }

  void schedule_decode(const std::shared_ptr<detail::asset_state>& state,
                       const std::shared_ptr<asset_loader>& loader, asset_location location,
                       std::shared_ptr<std::vector<std::byte>> document,
                       std::shared_ptr<dependency_batch> batch) {
    const std::weak_ptr weak_self{shared_from_this()};
    const auto posted = tasks.worker().post([weak_self, state, loader,
                                             location = std::move(location),
                                             document = std::move(document),
                                             batch = std::move(batch)]() mutable {
      const auto dependencies = batch ? std::span<const asset_dependency_data>{batch->dependencies}
                                      : std::span<const asset_dependency_data>{};
      auto decoded = loader->accepts(location, *document)
                         ? loader->decode(location, *document, dependencies)
                         : asset_decode_result{.error = asset_error::invalid_data,
                                               .value = {},
                                               .diagnostic = "Loader 拒绝资产内容"};
      const auto self = weak_self.lock();
      if (!self)
        return;
      static_cast<void>(self->tasks.main().post([state, decoded = std::move(decoded)]() mutable {
        if (!decoded.succeeded()) {
          fail(state, decoded.error, std::move(decoded.diagnostic));
          return;
        }
        std::scoped_lock lock{state->mutex};
        state->value = std::move(decoded.value);
        state->error = asset_error::none;
        state->diagnostic.clear();
        state->progress.stage = asset_stage::complete;
        state->progress.fraction = 1.0F;
        state->status.store(asset_status::ready, std::memory_order_release);
      }));
    });
    if (posted.failed()) {
      post_failure(state,
                   posted == granit::result::out_of_memory ? asset_error::out_of_memory
                                                           : asset_error::cancelled,
                   "无法提交资产解码任务");
    }
  }

  void process_source_result(const std::shared_ptr<detail::asset_state>& state,
                             const std::shared_ptr<asset_loader>& loader,
                             const asset_location& location, asset_source_result read) noexcept {
    if (!read.succeeded()) {
      post_failure(state, read.error, std::move(read.diagnostic));
      return;
    }
    try {
      auto document = std::make_shared<std::vector<std::byte>>(std::move(read.bytes));
      {
        std::scoped_lock lock{state->mutex};
        state->progress.completed_bytes = document->size();
        state->progress.total_bytes = read.total_bytes.value_or(document->size());
        state->progress.stage = asset_stage::decoding;
      }
      state->status.store(asset_status::decoding, std::memory_order_release);

      {
        std::scoped_lock lock{state->mutex};
        state->progress.stage = asset_stage::discovering_dependencies;
      }
      state->status.store(asset_status::discovering_dependencies, std::memory_order_release);
      auto discovery = loader->discover_dependencies(location, *document);
      if (!discovery.succeeded()) {
        post_failure(state, discovery.error, std::move(discovery.diagnostic));
        return;
      }
      {
        std::scoped_lock lock{state->mutex};
        state->progress.total_dependencies =
            static_cast<std::uint32_t>(discovery.dependencies.size());
      }
      if (discovery.dependencies.empty()) {
        schedule_decode(state, loader, location, std::move(document), {});
        return;
      }

      {
        std::scoped_lock lock{state->mutex};
        state->progress.stage = asset_stage::loading_dependencies;
      }
      state->status.store(asset_status::loading_dependencies, std::memory_order_release);

      auto batch = std::make_shared<dependency_batch>();
      batch->document = document;
      batch->dependencies.resize(discovery.dependencies.size());
      const std::weak_ptr weak_self{shared_from_this()};
      for (std::size_t index = 0; index < discovery.dependencies.size(); ++index) {
        const auto& uri = discovery.dependencies[index];
        auto dependency_location = location.resolve(uri);
        if (!dependency_location.valid()) {
          post_failure(state, asset_error::invalid_location, "资产依赖位置无效");
          return;
        }
        if (dependency_location.key() == location.key()) {
          post_failure(state, asset_error::invalid_data, "资产依赖图包含自循环");
          return;
        }
        std::shared_ptr<asset_source> source;
        {
          std::scoped_lock lock{mutex};
          const auto found = sources.find(dependency_location.scheme());
          if (found != sources.end())
            source = found->second;
        }
        if (!source) {
          post_failure(state, asset_error::source_not_registered, "资产依赖 Source 未注册");
          return;
        }
        const auto started =
            source->load(dependency_location, [weak_self, state, loader, location, batch, index,
                                               uri](asset_source_result result) mutable {
              const auto self = weak_self.lock();
              if (!self)
                return;
              bool decode = false;
              std::size_t completed_count{};
              asset_error error = asset_error::none;
              std::string diagnostic;
              {
                std::scoped_lock lock{batch->mutex};
                if (batch->failed)
                  return;
                if (!result.succeeded()) {
                  batch->failed = true;
                  error = result.error == asset_error::out_of_memory ? asset_error::out_of_memory
                                                                     : asset_error::dependency_read;
                  diagnostic = std::move(result.diagnostic);
                } else {
                  batch->dependencies[index] = {.uri = uri, .bytes = std::move(result.bytes)};
                  ++batch->completed;
                  completed_count = batch->completed;
                  decode = batch->completed == batch->dependencies.size();
                }
              }
              if (error != asset_error::none) {
                self->post_failure(state, error, std::move(diagnostic));
                return;
              }
              {
                std::scoped_lock lock{state->mutex};
                state->progress.completed_dependencies =
                    static_cast<std::uint32_t>(completed_count);
              }
              if (decode)
                self->schedule_decode(state, loader, location, batch->document, batch);
            });
        if (started.failed()) {
          {
            std::scoped_lock lock{batch->mutex};
            batch->failed = true;
          }
          post_failure(state,
                       started == granit::result::out_of_memory ? asset_error::out_of_memory
                                                                : asset_error::io_error,
                       "无法启动资产依赖读取");
          return;
        }
      }
    } catch (const std::bad_alloc&) {
      post_failure(state, asset_error::out_of_memory, "处理资产依赖时内存不足");
    } catch (...) {
      post_failure(state, asset_error::internal, "处理资产依赖时发生内部错误");
    }
  }

  tasks::task_system& tasks;
  std::mutex mutex;
  std::unordered_map<asset_scheme, std::shared_ptr<asset_source>> sources;
  std::unordered_map<std::type_index, std::shared_ptr<asset_loader>> loaders;
  std::unordered_map<request_key, std::shared_ptr<detail::asset_state>, request_key_hash> requests;
  std::uint64_t cache_hits{};
  bool registrations_locked{};
};

asset_manager::asset_manager(tasks::task_system& tasks)
    : implementation_(std::make_shared<implementation>(tasks)) {}

asset_manager::~asset_manager() = default;

granit::result asset_manager::register_source(asset_scheme scheme,
                                              std::shared_ptr<asset_source> source) noexcept {
  if (!source)
    return granit::result::invalid_argument;
  try {
    std::scoped_lock lock{implementation_->mutex};
    if (implementation_->registrations_locked || implementation_->sources.contains(scheme))
      return granit::result::invalid_argument;
    implementation_->sources.emplace(scheme, std::move(source));
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result asset_manager::register_loader(std::shared_ptr<asset_loader> loader) noexcept {
  if (!loader)
    return granit::result::invalid_argument;
  try {
    std::scoped_lock lock{implementation_->mutex};
    const auto type = loader->target_type();
    if (implementation_->registrations_locked || implementation_->loaders.contains(type))
      return granit::result::invalid_argument;
    implementation_->loaders.emplace(type, std::move(loader));
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

asset_manager::observation asset_manager::load_erased(std::type_index type,
                                                      asset_location location) noexcept {
  try {
    auto observer = std::make_shared<detail::asset_observer>();
    auto state = std::make_shared<detail::asset_state>();
    if (!location.valid()) {
      fail(state, asset_error::invalid_location, "资产位置无效");
      return {std::move(state), std::move(observer)};
    }

    std::shared_ptr<asset_source> source;
    std::shared_ptr<asset_loader> loader;
    const request_key key{.type = type, .location = location.key()};
    {
      std::scoped_lock lock{implementation_->mutex};
      implementation_->registrations_locked = true;
      if (const auto found = implementation_->requests.find(key);
          found != implementation_->requests.end()) {
        ++implementation_->cache_hits;
        return {found->second, std::move(observer)};
      }
      const auto source_it = implementation_->sources.find(location.scheme());
      if (source_it == implementation_->sources.end()) {
        fail(state, asset_error::source_not_registered, "资产 Source 未注册");
        return {std::move(state), std::move(observer)};
      }
      const auto loader_it = implementation_->loaders.find(type);
      if (loader_it == implementation_->loaders.end()) {
        fail(state, asset_error::loader_not_registered, "目标资产 Loader 未注册");
        return {std::move(state), std::move(observer)};
      }
      source = source_it->second;
      loader = loader_it->second;
      implementation_->requests.emplace(key, state);
    }

    const std::weak_ptr weak_manager{implementation_};
    const auto queued = implementation_->tasks.worker().post(
        [weak_manager, state, source = std::move(source), loader = std::move(loader),
         location = std::move(location)]() mutable {
          state->status.store(asset_status::reading, std::memory_order_release);
          {
            std::scoped_lock lock{state->mutex};
            state->progress.stage = asset_stage::reading;
          }
          const auto started = source->load(
              location, [weak_manager, state, loader, location](asset_source_result read) mutable {
                const auto manager = weak_manager.lock();
                if (!manager)
                  return;
                manager->process_source_result(state, loader, location, std::move(read));
              });
          if (started.failed()) {
            if (const auto manager = weak_manager.lock()) {
              static_cast<void>(manager->tasks.main().post([state, started] {
                fail(state,
                     started == granit::result::out_of_memory ? asset_error::out_of_memory
                                                              : asset_error::io_error,
                     "无法启动资产读取");
              }));
            }
          }
        });
    if (queued.failed()) {
      fail(state,
           queued == granit::result::out_of_memory ? asset_error::out_of_memory
                                                   : asset_error::cancelled,
           "无法提交资产读取任务");
    }
    return {std::move(state), std::move(observer)};
  } catch (const std::bad_alloc&) {
    return {};
  } catch (...) {
    return {};
  }
}

asset_group asset_manager::create_group() noexcept { return asset_group{*this}; }

asset_manager_stats asset_manager::stats() const noexcept {
  asset_manager_stats output;
  std::scoped_lock lock{implementation_->mutex};
  output.cache_hits = implementation_->cache_hits;
  for (const auto& [key, state] : implementation_->requests) {
    static_cast<void>(key);
    switch (state->status.load(std::memory_order_acquire)) {
    case asset_status::ready:
      ++output.cached_requests;
      break;
    case asset_status::failed:
    case asset_status::cancelled:
      ++output.failed_requests;
      break;
    case asset_status::queued:
    case asset_status::reading:
    case asset_status::discovering_dependencies:
    case asset_status::loading_dependencies:
    case asset_status::decoding:
      ++output.active_requests;
      break;
    }
  }
  return output;
}

void asset_manager::clear_cache() noexcept {
  std::scoped_lock lock{implementation_->mutex};
  implementation_->requests.clear();
}

} // namespace granit::example::assets
