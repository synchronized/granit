// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_ENVIRONMENT_MAP_HPP_
#define GRANIT_PIPELINE_ENVIRONMENT_MAP_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/environment_map.h>

#include <cstddef>
#include <span>
#include <utility>

namespace granit {

/** 公共 Environment Map C ABI 的轻量 move-only RAII 包装。 */
class environment_map {
public:
  environment_map() = default;
  ~environment_map() { static_cast<void>(reset()); }
  environment_map(const environment_map&) = delete;
  environment_map& operator=(const environment_map&) = delete;
  environment_map(environment_map&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  environment_map& operator=(environment_map&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  std::span<const std::byte> asset) noexcept {
    if (valid())
      return result::invalid_argument;
    const granit_environment_map_asset_desc desc{sizeof(granit_environment_map_asset_desc), 0,
                                                 asset.data(), asset.size()};
    const auto value =
        from_native(granit_environment_map_create_from_asset(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize_builtin(granit_renderer renderer) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_environment_map_create_builtin(renderer, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result get_info(granit_environment_map_info& info) const noexcept {
    info = GRANIT_ENVIRONMENT_MAP_INFO_INIT;
    return from_native(granit_environment_map_get_info(renderer_, handle_, &info));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_environment_map_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_environment_map native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_environment_map handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
