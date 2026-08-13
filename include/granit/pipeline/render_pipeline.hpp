// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_RENDER_PIPELINE_HPP_
#define GRANIT_PIPELINE_RENDER_PIPELINE_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/render_pipeline.h>

#include <utility>

namespace granit {

/** 统一参考渲染管线 C ABI 的轻量 move-only RAII 包装。 */
class render_pipeline {
public:
  render_pipeline() = default;
  ~render_pipeline() { static_cast<void>(reset()); }
  render_pipeline(const render_pipeline&) = delete;
  render_pipeline& operator=(const render_pipeline&) = delete;
  render_pipeline(render_pipeline&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  render_pipeline& operator=(render_pipeline&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_render_pipeline_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_render_pipeline_create(renderer, &desc, &handle_));
    if (succeeded(value))
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result render(const granit_render_pipeline_render_desc& desc) noexcept {
    return from_native(granit_render_pipeline_render(renderer_, handle_, &desc));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    return from_native(granit_render_pipeline_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_render_pipeline native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_render_pipeline handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
