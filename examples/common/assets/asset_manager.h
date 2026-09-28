// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_MANAGER_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_MANAGER_H_

#include "assets/asset_group.h"
#include "assets/asset_handle.h"
#include "assets/asset_loader.h"
#include "assets/asset_location.h"
#include "assets/asset_source.h"
#include "tasks/task_system.h"

#include <memory>
#include <typeindex>

namespace granit::example::assets {

struct asset_manager_stats {
  std::uint64_t active_requests{};
  std::uint64_t cached_requests{};
  std::uint64_t failed_requests{};
  std::uint64_t cache_hits{};
};

/** 示例私有的异步资产加载、请求合并和进程内缓存入口。 */
class asset_manager final {
public:
  explicit asset_manager(tasks::task_system& tasks);
  ~asset_manager();
  asset_manager(const asset_manager&) = delete;
  asset_manager& operator=(const asset_manager&) = delete;

  [[nodiscard]] granit::result
  register_source(asset_scheme scheme, std::shared_ptr<asset_source> source) noexcept;
  [[nodiscard]] granit::result register_loader(std::shared_ptr<asset_loader> loader) noexcept;

  template <typename T> [[nodiscard]] asset_handle<T> load(asset_location location) noexcept {
    auto observation = load_erased(typeid(T), std::move(location));
    return {std::move(observation.state), std::move(observation.observer)};
  }

  [[nodiscard]] asset_group create_group() noexcept;
  [[nodiscard]] asset_manager_stats stats() const noexcept;

  void clear_cache() noexcept;

private:
  struct observation {
    std::shared_ptr<detail::asset_state> state;
    std::shared_ptr<detail::asset_observer> observer;
  };
  struct implementation;

  [[nodiscard]] observation load_erased(std::type_index type, asset_location location) noexcept;

  std::shared_ptr<implementation> implementation_;
};

} // namespace granit::example::assets

template <typename T>
granit::example::assets::asset_handle<T>
granit::example::assets::asset_group::load(asset_location location) noexcept {
  if (manager_ == nullptr)
    return {};
  auto handle = manager_->load<T>(std::move(location));
  observe(handle.state_, handle.observer_);
  return handle;
}

#endif
