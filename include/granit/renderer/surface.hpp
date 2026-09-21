// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SURFACE_HPP_
#define GRANIT_SURFACE_HPP_

#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.h>

namespace granit {

class surface_desc;

/** 无异常、move-only 的 Surface RAII 包装。 */
class surface {
public:
  surface() = default;
  ~surface() { static_cast<void>(reset()); }

  surface(const surface&) = delete;
  surface& operator=(const surface&) = delete;

  surface(surface&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}

  surface& operator=(surface&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer, const surface_desc& desc) noexcept;
  [[nodiscard]] result initialize(renderer& owner, const surface_desc& desc) noexcept;

  [[nodiscard]] result reset() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto value = granit_surface_destroy(renderer_, handle_);
    if (value == GRANIT_SUCCESS || value == GRANIT_ERROR_INVALID_HANDLE) {
      renderer_ = GRANIT_NULL_HANDLE;
      handle_ = GRANIT_NULL_HANDLE;
    }
    return from_native(value);
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_surface native_handle() const noexcept { return handle_; }
  [[nodiscard]] granit_renderer renderer_handle() const noexcept { return renderer_; }

private:
  friend class window;

  void adopt(granit_renderer renderer, granit_surface handle) noexcept {
    renderer_ = renderer;
    handle_ = handle;
  }

  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_surface handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
