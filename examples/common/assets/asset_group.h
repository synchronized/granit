// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_GROUP_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_GROUP_H_

#include "assets/asset_handle.h"
#include "assets/asset_location.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace granit::example::assets {

class asset_manager;

struct asset_group_progress {
  std::uint32_t total_assets{};
  std::uint32_t completed_assets{};
  std::uint32_t failed_assets{};
  std::uint32_t cancelled_assets{};
  std::uint64_t completed_bytes{};
  std::optional<std::uint64_t> total_bytes;
  std::uint32_t completed_dependencies{};
  std::optional<std::uint32_t> total_dependencies;
  std::optional<float> fraction;
};

/** 汇总一批根资产请求；复制出的 Handle 与 Group 共享同一观察状态。 */
class asset_group final {
public:
  asset_group() = default;

  template <typename T> [[nodiscard]] asset_handle<T> load(asset_location location) noexcept;

  [[nodiscard]] asset_group_progress progress() const noexcept;
  [[nodiscard]] bool complete() const noexcept;
  void cancel() noexcept;

private:
  friend class asset_manager;
  struct implementation;

  explicit asset_group(asset_manager& manager) noexcept;
  void observe(const std::shared_ptr<detail::asset_state>& state,
               const std::shared_ptr<detail::asset_observer>& observer) noexcept;

  asset_manager* manager_{};
  std::shared_ptr<implementation> implementation_;
};

} // namespace granit::example::assets

#endif
