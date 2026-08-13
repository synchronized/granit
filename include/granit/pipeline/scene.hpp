// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_SCENE_HPP_
#define GRANIT_PIPELINE_SCENE_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/scene.h>

#include <utility>

namespace granit {

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
    if (succeeded(value))
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    return from_native(granit_scene_snapshot_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_scene_snapshot native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_scene_snapshot handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
