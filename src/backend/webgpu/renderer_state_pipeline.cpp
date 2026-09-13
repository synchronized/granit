// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <utility>
#include <vector>

namespace granit::detail {

namespace {

class webgpu_pipeline_warmup_completion final : public backend_pipeline_warmup_completion {
public:
  webgpu_pipeline_warmup_completion(std::shared_ptr<webgpu_renderer_state> renderer,
                                    webgpu_pipeline_warmup warmup) noexcept
      : renderer_(std::move(renderer)), warmup_(warmup) {}
  ~webgpu_pipeline_warmup_completion() override {
    if (warmup_ != 0)
      static_cast<void>(renderer_->native_device().destroy_pipeline_warmup(warmup_));
  }

  granit_result poll() noexcept override {
    return renderer_->native_device().poll_pipeline_warmup(warmup_);
  }

private:
  std::shared_ptr<webgpu_renderer_state> renderer_;
  webgpu_pipeline_warmup warmup_{};
};

} // namespace

std::unique_ptr<backend_compute_pipeline_resource>
webgpu_renderer_state::allocate_compute_pipeline_resource() {
  return capabilities_initialized_ ? allocate_compute_pipeline() : nullptr;
}

granit_result webgpu_renderer_state::create_compute_pipeline(
    backend_pipeline_layout_resource& layout, backend_shader_resource& shader, const char*,
    backend_compute_pipeline_resource& pipeline) noexcept {
  if (!capabilities_initialized_)
    return GRANIT_ERROR_UNSUPPORTED;
  return create_compute_pipeline(pipeline, native_pipeline_layout(layout), native_shader(shader));
}

granit_result webgpu_renderer_state::warmup_compute_pipeline_async(
    backend_pipeline_layout_resource& layout, backend_shader_resource& shader, const char*,
    std::unique_ptr<backend_pipeline_warmup_completion>& completion) noexcept {
  if (!capabilities_initialized_)
    return GRANIT_ERROR_UNSUPPORTED;
  webgpu_pipeline_warmup warmup{};
  const auto result =
      begin_compute_pipeline_warmup(native_pipeline_layout(layout), native_shader(shader), warmup);
  if (result != GRANIT_SUCCESS)
    return result;
  try {
    completion = std::make_unique<webgpu_pipeline_warmup_completion>(shared_from_this(), warmup);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    static_cast<void>(device_.destroy_pipeline_warmup(warmup));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

std::unique_ptr<backend_pipeline_layout_resource>
webgpu_renderer_state::allocate_pipeline_layout_resource() {
  return capabilities_initialized_ ? allocate_pipeline_layout() : nullptr;
}

granit_result webgpu_renderer_state::create_pipeline_layout(
    std::span<backend_bind_group_layout_resource* const> bind_group_layouts,
    backend_pipeline_layout_resource& layout) noexcept {
  if (!capabilities_initialized_)
    return GRANIT_ERROR_UNSUPPORTED;
  try {
    std::vector<webgpu_bind_group_layout> native_layouts;
    native_layouts.reserve(bind_group_layouts.size());
    for (auto* bind_group_layout : bind_group_layouts) {
      if (bind_group_layout == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto native = native_bind_group_layout(*bind_group_layout);
      if (native == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_layouts.push_back(native);
    }
    return create_pipeline_layout(native_layouts, layout);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

std::unique_ptr<backend_graphics_pipeline_resource>
webgpu_renderer_state::allocate_graphics_pipeline_resource() {
  return capabilities_initialized_ ? allocate_graphics_pipeline() : nullptr;
}

granit_result webgpu_renderer_state::create_graphics_pipeline(
    const backend_graphics_pipeline_create_info& info,
    backend_graphics_pipeline_resource& pipeline) noexcept {
  if (!capabilities_initialized_ || info.color_formats.size() > 1)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto color_format =
      info.color_formats.empty() ? GRANIT_TEXTURE_FORMAT_UNDEFINED : info.color_formats.front();
  const granit_color_blend_state default_blend = GRANIT_COLOR_BLEND_STATE_INIT;
  const auto& color_blend = info.color_blends.empty() ? default_blend : info.color_blends.front();
  return create_graphics_pipeline(pipeline, info.layout, native_shader(info.vertex_shader),
                                  native_shader(info.fragment_shader), info.vertex_buffers,
                                  color_format, info.depth_stencil_format, info.sample_count,
                                  info.primitive, info.depth, info.depth_bias, color_blend);
}

granit_result webgpu_renderer_state::warmup_graphics_pipeline_async(
    const backend_graphics_pipeline_create_info& info,
    std::unique_ptr<backend_pipeline_warmup_completion>& completion) noexcept {
  if (!capabilities_initialized_ || info.color_formats.size() > 1)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto color_format =
      info.color_formats.empty() ? GRANIT_TEXTURE_FORMAT_UNDEFINED : info.color_formats.front();
  const granit_color_blend_state default_blend = GRANIT_COLOR_BLEND_STATE_INIT;
  const auto& color_blend = info.color_blends.empty() ? default_blend : info.color_blends.front();
  webgpu_pipeline_warmup warmup{};
  const auto result = begin_graphics_pipeline_warmup(
      info.layout, native_shader(info.vertex_shader), native_shader(info.fragment_shader),
      info.vertex_buffers, color_format, info.depth_stencil_format, info.sample_count,
      info.primitive, info.depth, info.depth_bias, color_blend, warmup);
  if (result != GRANIT_SUCCESS)
    return result;
  try {
    completion = std::make_unique<webgpu_pipeline_warmup_completion>(shared_from_this(), warmup);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    static_cast<void>(device_.destroy_pipeline_warmup(warmup));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

} // namespace granit::detail
