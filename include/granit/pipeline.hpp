// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_HPP_
#define GRANIT_PIPELINE_HPP_

#include <span>
#include <utility>

#include <granit/pipeline.h>
#include <granit/resource_types.hpp>
#include <granit/result.hpp>

namespace granit {

class pipeline_layout {
public:
  pipeline_layout() = default;
  ~pipeline_layout() { static_cast<void>(reset()); }
  pipeline_layout(const pipeline_layout&) = delete;
  pipeline_layout& operator=(const pipeline_layout&) = delete;
  pipeline_layout(pipeline_layout&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  pipeline_layout& operator=(pipeline_layout&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer) noexcept;
  [[nodiscard]] result reset() noexcept;
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_pipeline_layout native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_pipeline_layout handle_{GRANIT_NULL_HANDLE};
};

struct graphics_pipeline_desc {
  granit_pipeline_layout layout{GRANIT_NULL_HANDLE};
  granit_shader vertex_shader{GRANIT_NULL_HANDLE};
  granit_shader fragment_shader{GRANIT_NULL_HANDLE};
  std::span<const texture_format> color_formats;
  texture_format depth_stencil_format{texture_format::undefined};
  sample_count samples{sample_count::one};
};

class graphics_pipeline {
public:
  graphics_pipeline() = default;
  ~graphics_pipeline() { static_cast<void>(reset()); }
  graphics_pipeline(const graphics_pipeline&) = delete;
  graphics_pipeline& operator=(const graphics_pipeline&) = delete;
  graphics_pipeline(graphics_pipeline&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  graphics_pipeline& operator=(graphics_pipeline&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const graphics_pipeline_desc& desc) noexcept;
  [[nodiscard]] result reset() noexcept;
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_graphics_pipeline native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_graphics_pipeline handle_{GRANIT_NULL_HANDLE};
};

inline result pipeline_layout::initialize(granit_renderer renderer) noexcept {
  if (valid() || renderer == GRANIT_NULL_HANDLE)
    return result::invalid_argument;
  const granit_pipeline_layout_desc desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
  const auto value = granit_pipeline_layout_create(renderer, &desc, &handle_);
  if (value == GRANIT_SUCCESS)
    renderer_ = renderer;
  return from_native(value);
}

inline result pipeline_layout::reset() noexcept {
  if (!valid())
    return result::success;
  const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
  const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
  return from_native(granit_pipeline_layout_destroy(renderer, handle));
}

inline result graphics_pipeline::initialize(granit_renderer renderer,
                                            const graphics_pipeline_desc& desc) noexcept {
  if (valid() || renderer == GRANIT_NULL_HANDLE || desc.color_formats.size() > UINT32_MAX)
    return result::invalid_argument;
  const auto* formats = reinterpret_cast<const granit_texture_format*>(desc.color_formats.data());
  const granit_graphics_pipeline_desc native{
      .struct_size = GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_1_SIZE,
      .reserved = 0,
      .layout = desc.layout,
      .vertex_shader = desc.vertex_shader,
      .fragment_shader = desc.fragment_shader,
      .color_format_count = static_cast<std::uint32_t>(desc.color_formats.size()),
      .color_formats = formats,
      .depth_stencil_format = static_cast<granit_texture_format>(desc.depth_stencil_format),
      .sample_count = static_cast<granit_sample_count>(desc.samples),
      .reserved_2 = 0};
  const auto value = granit_graphics_pipeline_create(renderer, &native, &handle_);
  if (value == GRANIT_SUCCESS)
    renderer_ = renderer;
  return from_native(value);
}

inline result graphics_pipeline::reset() noexcept {
  if (!valid())
    return result::success;
  const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
  const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
  return from_native(granit_graphics_pipeline_destroy(renderer, handle));
}

} // namespace granit

#endif
