// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline_validation.h"

#include <cstdio>

namespace granit::example::model_viewer::web {

granit_result pipeline_validation::begin(granit_renderer renderer) {
  if (renderer == GRANIT_NULL_HANDLE || phase_ != phase::idle)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  renderer_ = renderer;
  constexpr char vertex_wgsl[] = R"(
@vertex fn main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  var positions = array<vec2f, 3>(vec2f(0.0, 0.5), vec2f(-0.5, -0.5), vec2f(0.5, -0.5));
  return vec4f(positions[index], 0.0, 1.0);
})";
  constexpr char fragment_wgsl[] = R"(
@fragment fn main() -> @location(0) vec4f {
  return vec4f(0.0, 1.0, 0.0, 1.0);
})";
  constexpr char compute_wgsl[] = R"(
@compute @workgroup_size(1) fn main() {}
)";

  granit_shader_desc shader_desc = GRANIT_SHADER_DESC_INIT;
  shader_desc.code_format = GRANIT_SHADER_CODE_FORMAT_WGSL;
  shader_desc.code = vertex_wgsl;
  shader_desc.code_size = sizeof(vertex_wgsl) - 1;
  auto result = granit_shader_create(renderer_, &shader_desc, &vertex_);
  if (result == GRANIT_SUCCESS) {
    shader_desc.stage = GRANIT_SHADER_STAGE_FRAGMENT;
    shader_desc.code = fragment_wgsl;
    shader_desc.code_size = sizeof(fragment_wgsl) - 1;
    result = granit_shader_create(renderer_, &shader_desc, &fragment_);
  }
  if (result == GRANIT_SUCCESS) {
    const granit_pipeline_layout_desc layout_desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
    result = granit_pipeline_layout_create(renderer_, &layout_desc, &layout_);
  }
  if (result == GRANIT_SUCCESS) {
    shader_desc.stage = GRANIT_SHADER_STAGE_COMPUTE;
    shader_desc.code = compute_wgsl;
    shader_desc.code_size = sizeof(compute_wgsl) - 1;
    result = granit_shader_create(renderer_, &shader_desc, &compute_);
    if (result != GRANIT_SUCCESS)
      std::fprintf(stderr, "GRANIT_DIAGNOSTIC:Compute Shader 创建失败：%d\n", result);
  }

  constexpr granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  granit_graphics_pipeline_desc pipeline_desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  pipeline_desc.layout = layout_;
  pipeline_desc.vertex_shader = vertex_;
  pipeline_desc.fragment_shader = fragment_;
  pipeline_desc.color_format_count = 1;
  pipeline_desc.color_formats = &color_format;

  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  if (result == GRANIT_SUCCESS)
    result = granit_renderer_get_limits(renderer_, &limits);
  if (result == GRANIT_SUCCESS &&
      (limits.supported_features & GRANIT_RENDERER_FEATURE_NON_BLOCKING_PIPELINE_WARMUP_BIT) == 0)
    result = GRANIT_ERROR_UNSUPPORTED;
  if (result == GRANIT_SUCCESS) {
    const granit_pipeline_warmup_batch_desc batch_desc = GRANIT_PIPELINE_WARMUP_BATCH_DESC_INIT;
    result = granit_pipeline_warmup_batch_create(renderer_, &batch_desc, &batch_);
  }
  if (result == GRANIT_SUCCESS) {
    result = granit_pipeline_warmup_batch_add_graphics(renderer_, batch_, &pipeline_desc,
                                                       &graphics_index_);
  }

  granit_compute_pipeline_desc compute_desc = GRANIT_COMPUTE_PIPELINE_DESC_INIT;
  compute_desc.layout = layout_;
  compute_desc.compute_shader = compute_;
  if (result == GRANIT_SUCCESS) {
    result =
        granit_pipeline_warmup_batch_add_compute(renderer_, batch_, &compute_desc, &compute_index_);
  }
  if (result != GRANIT_SUCCESS)
    std::fprintf(stderr, "GRANIT_DIAGNOSTIC:Pipeline 预热批次构建失败：%d\n", result);
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_submit_async(renderer_, batch_, &operation_);

  if (result != GRANIT_SUCCESS) {
    reset();
    return result;
  }
  phase_ = phase::running;
  return GRANIT_SUCCESS;
}

granit_result pipeline_validation::poll() {
  if (phase_ == phase::complete)
    return GRANIT_SUCCESS;
  if (phase_ != phase::running)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  granit_async_operation_status status = GRANIT_ASYNC_OPERATION_STATUS_INIT;
  auto result = granit_async_operation_get_status(renderer_, operation_, &status);
  if (result == GRANIT_SUCCESS && status.state == GRANIT_ASYNC_OPERATION_STATE_RUNNING)
    return GRANIT_ERROR_NOT_READY;

  granit_pipeline_warmup_result_info graphics_info = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;
  granit_pipeline_warmup_result_info compute_info = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;
  bool software_adapter_fallback{};
  const auto accept_software_adapter_failure = [&software_adapter_fallback](granit_result value) {
    if (value == GRANIT_ERROR_INTERNAL) {
      software_adapter_fallback = true;
      return GRANIT_SUCCESS;
    }
    return value;
  };
  if (result == GRANIT_SUCCESS && status.state == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED) {
    result = granit_pipeline_warmup_operation_get_result(renderer_, operation_, graphics_index_,
                                                         &graphics_info);
    if (result == GRANIT_SUCCESS) {
      result = granit_pipeline_warmup_operation_get_result(renderer_, operation_, compute_index_,
                                                           &compute_info);
    }
    if (result == GRANIT_SUCCESS)
      result = accept_software_adapter_failure(graphics_info.result);
    if (result == GRANIT_SUCCESS)
      result = accept_software_adapter_failure(compute_info.result);
  } else if (result == GRANIT_SUCCESS) {
    result = status.result == GRANIT_ERROR_NOT_READY ? GRANIT_ERROR_NOT_READY : status.result;
  }
  if (software_adapter_fallback) {
    std::fprintf(stderr,
                 "GRANIT_DIAGNOSTIC:软件 WebGPU 适配器异步编译失败，回退到按需同步创建管线\n");
  }
  if (result != GRANIT_SUCCESS) {
    reset();
    return result;
  }

  result = granit_async_operation_destroy(renderer_, operation_);
  operation_ = GRANIT_NULL_HANDLE;
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_destroy(renderer_, batch_);
  batch_ = GRANIT_NULL_HANDLE;
  if (result != GRANIT_SUCCESS) {
    reset();
    return result;
  }

  granit_graphics_pipeline pipeline{};
  constexpr granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  granit_graphics_pipeline_desc pipeline_desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  pipeline_desc.layout = layout_;
  pipeline_desc.vertex_shader = vertex_;
  pipeline_desc.fragment_shader = fragment_;
  pipeline_desc.color_format_count = 1;
  pipeline_desc.color_formats = &color_format;
  result = granit_graphics_pipeline_create(renderer_, &pipeline_desc, &pipeline);
  if (result != GRANIT_SUCCESS) {
    reset();
    return result;
  }
  if (granit_shader_destroy(renderer_, compute_) != GRANIT_SUCCESS ||
      granit_shader_destroy(renderer_, vertex_) != GRANIT_SUCCESS ||
      granit_pipeline_layout_destroy(renderer_, layout_) != GRANIT_SUCCESS) {
    static_cast<void>(granit_graphics_pipeline_destroy(renderer_, pipeline));
    reset();
    return GRANIT_ERROR_INTERNAL;
  }
  compute_ = GRANIT_NULL_HANDLE;
  vertex_ = GRANIT_NULL_HANDLE;
  layout_ = GRANIT_NULL_HANDLE;
  result = granit_graphics_pipeline_destroy(renderer_, pipeline);
  if (result == GRANIT_SUCCESS) {
    result = granit_shader_destroy(renderer_, fragment_);
    if (result == GRANIT_SUCCESS)
      fragment_ = GRANIT_NULL_HANDLE;
  }
  if (result != GRANIT_SUCCESS ||
      granit_shader_destroy(renderer_, vertex_) != GRANIT_ERROR_INVALID_HANDLE) {
    reset();
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INTERNAL : result;
  }

  phase_ = phase::complete;
  return GRANIT_SUCCESS;
}

void pipeline_validation::reset() noexcept {
  if (renderer_ != GRANIT_NULL_HANDLE) {
    if (operation_ != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_async_operation_destroy(renderer_, operation_));
    if (batch_ != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_pipeline_warmup_batch_destroy(renderer_, batch_));
    if (layout_ != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_pipeline_layout_destroy(renderer_, layout_));
    if (compute_ != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_shader_destroy(renderer_, compute_));
    if (fragment_ != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_shader_destroy(renderer_, fragment_));
    if (vertex_ != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_shader_destroy(renderer_, vertex_));
  }
  renderer_ = GRANIT_NULL_HANDLE;
  vertex_ = GRANIT_NULL_HANDLE;
  fragment_ = GRANIT_NULL_HANDLE;
  compute_ = GRANIT_NULL_HANDLE;
  layout_ = GRANIT_NULL_HANDLE;
  batch_ = GRANIT_NULL_HANDLE;
  operation_ = GRANIT_NULL_HANDLE;
  graphics_index_ = 0;
  compute_index_ = 0;
  phase_ = phase::idle;
}

} // namespace granit::example::model_viewer::web
