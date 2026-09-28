// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_manager.h"

#include <mutex>
#include <new>
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
  state->error = error;
  state->diagnostic = std::move(diagnostic);
  state->progress.stage = asset_stage::complete;
  state->status.store(asset_status::failed, std::memory_order_release);
}

} // namespace

struct asset_manager::implementation {
  explicit implementation(tasks::task_system& task_system) : tasks{task_system} {}

  tasks::task_system& tasks;
  std::mutex mutex;
  std::unordered_map<asset_scheme, std::shared_ptr<asset_manager_source>> sources;
  std::unordered_map<std::type_index, std::shared_ptr<asset_loader>> loaders;
  std::unordered_map<request_key, std::shared_ptr<detail::asset_state>, request_key_hash> requests;
  bool registrations_locked{};
};

asset_manager::asset_manager(tasks::task_system& tasks)
    : implementation_(std::make_shared<implementation>(tasks)) {}

asset_manager::~asset_manager() = default;

granit::result asset_manager::register_source(asset_scheme scheme,
                                              std::shared_ptr<asset_manager_source> source) noexcept {
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

    std::shared_ptr<asset_manager_source> source;
    std::shared_ptr<asset_loader> loader;
    const request_key key{.type = type, .location = location.key()};
    {
      std::scoped_lock lock{implementation_->mutex};
      implementation_->registrations_locked = true;
      if (const auto found = implementation_->requests.find(key);
          found != implementation_->requests.end()) {
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
                if (!read.succeeded()) {
                  static_cast<void>(manager->tasks.main().post(
                      [state, error = read.error, diagnostic = std::move(read.diagnostic)]() mutable {
                        fail(state, error, std::move(diagnostic));
                      }));
                  return;
                }
                {
                  std::scoped_lock lock{state->mutex};
                  state->progress.completed_bytes = read.bytes.size();
                  state->progress.total_bytes = read.total_bytes.value_or(read.bytes.size());
                  state->progress.stage = asset_stage::decoding;
                }
                state->status.store(asset_status::decoding, std::memory_order_release);
                const auto posted = manager->tasks.worker().post(
                    [weak_manager, state, loader, location,
                     bytes = std::move(read.bytes)]() mutable {
                      auto decoded = loader->accepts(location, bytes)
                                         ? loader->decode(location, bytes)
                                         : asset_decode_result{
                                               .error = asset_error::invalid_data,
                                               .value = {},
                                               .diagnostic = "Loader 拒绝资产内容"};
                      const auto manager = weak_manager.lock();
                      if (!manager)
                        return;
                      static_cast<void>(manager->tasks.main().post(
                          [state, decoded = std::move(decoded)]() mutable {
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
                            state->status.store(asset_status::ready,
                                                std::memory_order_release);
                          }));
                    });
                if (posted.failed()) {
                  static_cast<void>(manager->tasks.main().post([state, posted] {
                    fail(state,
                         posted == granit::result::out_of_memory ? asset_error::out_of_memory
                                                                  : asset_error::cancelled,
                         "无法提交资产解码任务");
                  }));
                }
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
      fail(state, queued == granit::result::out_of_memory ? asset_error::out_of_memory
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

void asset_manager::clear_cache() noexcept {
  std::scoped_lock lock{implementation_->mutex};
  implementation_->requests.clear();
}

} // namespace granit::example::assets
