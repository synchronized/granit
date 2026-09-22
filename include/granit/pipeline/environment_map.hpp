// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_ENVIRONMENT_MAP_HPP_
#define GRANIT_PIPELINE_ENVIRONMENT_MAP_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/environment_map.h>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/renderer/renderer.hpp>

#include <cstddef>
#include <span>
#include <utility>

namespace granit {

struct environment_map_info {
  render_pipeline_environment environment;
  float recommended_exposure_ev{};
};

class environment_map;

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

  [[nodiscard]] result initialize(renderer_ref owner,
                                  std::span<const std::byte> asset) noexcept {
    const auto renderer = owner.native_handle();
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
  [[nodiscard]] result initialize(renderer& owner, std::span<const std::byte> asset) noexcept {
    return initialize(owner.ref(), asset);
  }
  [[nodiscard]] result initialize_builtin(renderer_ref owner) noexcept {
    const auto renderer = owner.native_handle();
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_environment_map_create_builtin(renderer, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize_builtin(renderer& owner) noexcept {
    return initialize_builtin(owner.ref());
  }
  [[nodiscard]] result get_info(environment_map_info& info) const noexcept {
    granit_environment_map_info native = GRANIT_ENVIRONMENT_MAP_INFO_INIT;
    const auto value = from_native(granit_environment_map_get_info(renderer_, handle_, &native));
    if (value.ok()) {
      info = {
          .environment = {.irradiance =
                              texture_view_ref::from_native(native.environment.irradiance),
                          .prefiltered_environment = texture_view_ref::from_native(
                              native.environment.prefiltered_environment),
                          .brdf_lut = texture_view_ref::from_native(native.environment.brdf_lut),
                          .rotation_radians = native.environment.rotation_radians,
                          .intensity = native.environment.intensity,
                          .prefiltered_max_mip = native.environment.prefiltered_max_mip},
          .recommended_exposure_ev = native.recommended_exposure_ev};
    }
    return value;
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
