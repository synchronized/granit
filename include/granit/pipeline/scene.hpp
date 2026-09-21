// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_SCENE_HPP_
#define GRANIT_PIPELINE_SCENE_HPP_

#include <granit/core/result.hpp>
#include <granit/math/types.hpp>
#include <granit/pipeline/scene.h>
#include <granit/renderer/renderer.hpp>

#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace granit {

using scene_view = ::granit_scene_view;
using scene_renderable = ::granit_scene_renderable;
using scene_directional_light = ::granit_scene_directional_light;
using scene_point_light = ::granit_scene_point_light;
using scene_spot_light = ::granit_scene_spot_light;

struct scene_snapshot_desc {
  std::span<const scene_view> views;
  std::span<const scene_renderable> renderables;
  std::span<const scene_directional_light> directional_lights;
  std::span<const scene_point_light> point_lights;
  std::span<const scene_spot_light> spot_lights;
};

class scene_snapshot;

/** 不拥有 Scene Snapshot，只在来源 Snapshot 的有效期内使用。 */
class scene_snapshot_ref {
public:
  scene_snapshot_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_scene_snapshot native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr scene_snapshot_ref
  from_native(granit_scene_snapshot handle) noexcept {
    return scene_snapshot_ref{handle};
  }

private:
  friend class scene_snapshot;

  explicit constexpr scene_snapshot_ref(granit_scene_snapshot handle) noexcept : handle_(handle) {}

  granit_scene_snapshot handle_{GRANIT_NULL_HANDLE};
};

/** Scene Snapshot 的轻量 move-only RAII 包装。 */
class scene_snapshot {
public:
  scene_snapshot() = default;
  ~scene_snapshot() { static_cast<void>(reset()); }
  scene_snapshot(const scene_snapshot&) = delete;
  scene_snapshot& operator=(const scene_snapshot&) = delete;
  scene_snapshot(scene_snapshot&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  scene_snapshot& operator=(scene_snapshot&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_scene_snapshot_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_scene_snapshot_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize(renderer& owner,
                                  const granit_scene_snapshot_desc& desc) noexcept {
    return initialize(owner.native_handle(), desc);
  }
  [[nodiscard]] result initialize(renderer& owner, const scene_snapshot_desc& desc) noexcept {
    if (desc.views.size() > std::numeric_limits<std::uint32_t>::max() ||
        desc.renderables.size() > std::numeric_limits<std::uint32_t>::max() ||
        desc.directional_lights.size() > std::numeric_limits<std::uint32_t>::max() ||
        desc.point_lights.size() > std::numeric_limits<std::uint32_t>::max() ||
        desc.spot_lights.size() > std::numeric_limits<std::uint32_t>::max()) {
      return result::invalid_argument;
    }
    const granit_scene_snapshot_desc native{
        .struct_size = GRANIT_SCENE_SNAPSHOT_DESC_VERSION_1_SIZE,
        .reserved = 0,
        .views = desc.views.data(),
        .view_count = static_cast<std::uint32_t>(desc.views.size()),
        .renderables = desc.renderables.data(),
        .renderable_count = static_cast<std::uint32_t>(desc.renderables.size()),
        .directional_lights = desc.directional_lights.data(),
        .directional_light_count = static_cast<std::uint32_t>(desc.directional_lights.size()),
        .point_lights = desc.point_lights.data(),
        .point_light_count = static_cast<std::uint32_t>(desc.point_lights.size()),
        .spot_lights = desc.spot_lights.data(),
        .spot_light_count = static_cast<std::uint32_t>(desc.spot_lights.size()),
    };
    return initialize(owner.native_handle(), native);
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    return from_native(granit_scene_snapshot_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr scene_snapshot_ref ref() const noexcept {
    return scene_snapshot_ref{handle_};
  }
  [[nodiscard]] granit_scene_snapshot native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_scene_snapshot handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
