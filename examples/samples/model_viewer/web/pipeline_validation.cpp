// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline_validation.h"

#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/texture.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <span>

namespace granit::example::model_viewer::web {

granit_result pipeline_validation::begin(granit::renderer_ref renderer) {
  if (!renderer || phase_ != phase::idle)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  renderer_ = renderer.native_handle();
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
  phase_ = phase::warmup;
  return GRANIT_SUCCESS;
}

granit_result pipeline_validation::begin_capability_readback() {
  const auto owner = granit::renderer_ref::from_native(renderer_);
  constexpr char vertex_wgsl[] = R"(
@vertex fn main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  var positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(positions[index], 0.0, 1.0);
})";
  constexpr char fragment_wgsl[] = R"(
struct output {
  @location(0) first: vec4f,
  @location(1) second: vec4f,
};
@fragment fn main() -> output {
  return output(vec4f(1.0, 0.0, 0.0, 1.0), vec4f(0.0, 1.0, 0.0, 1.0));
})";
  constexpr char compute_wgsl[] = R"(
@group(0) @binding(0) var output_texture: texture_storage_2d<rgba8unorm, write>;
@compute @workgroup_size(1) fn main(@builtin(global_invocation_id) id: vec3u) {
  textureStore(output_texture, vec2i(id.xy), vec4f(0.25, 0.5, 0.75, 1.0));
})";
  const auto shader_code = [](const char* source, std::size_t size) {
    return std::span{reinterpret_cast<const std::byte*>(source), size};
  };

  granit::shader vertex;
  granit::shader fragment;
  granit::shader compute;
  auto result =
      vertex.initialize(owner, {.stage = granit::shader_stage::vertex,
                                .code_format = granit::shader_code_format::wgsl,
                                .code = shader_code(vertex_wgsl, sizeof(vertex_wgsl) - 1)});
  if (result.ok())
    result =
        fragment.initialize(owner, {.stage = granit::shader_stage::fragment,
                                    .code_format = granit::shader_code_format::wgsl,
                                    .code = shader_code(fragment_wgsl, sizeof(fragment_wgsl) - 1)});
  if (result.ok())
    result =
        compute.initialize(owner, {.stage = granit::shader_stage::compute,
                                   .code_format = granit::shader_code_format::wgsl,
                                   .code = shader_code(compute_wgsl, sizeof(compute_wgsl) - 1)});

  granit::pipeline_layout graphics_layout;
  if (result.ok())
    result = graphics_layout.initialize(owner, {});
  constexpr std::array color_formats{granit::texture_format::rgba8_unorm,
                                     granit::texture_format::rgba8_unorm};
  granit::graphics_pipeline graphics_pipeline;
  if (result.ok()) {
    result = graphics_pipeline.initialize(
        owner, {.layout = graphics_layout.ref(),
                .vertex_shader = vertex.ref(),
                .fragment_shader = fragment.ref(),
                .color_formats = color_formats,
                .depth_stencil_format = granit::texture_format::undefined,
                .samples = granit::sample_count::one,
                .vertex_buffers = {},
                .primitive = {},
                .depth = std::nullopt,
                .color_blends = {},
                .depth_bias = std::nullopt});
  }

  const granit::bind_group_layout_entry storage_declaration{
      .binding = 0,
      .type = granit::binding_type::storage_texture,
      .visibility = granit::shader_stage_flags::compute,
      .storage_texture_format = granit::texture_format::rgba8_unorm,
      .storage_access = granit::storage_texture_access::write_only};
  granit::bind_group_layout storage_group_layout;
  if (result.ok())
    result = storage_group_layout.initialize(owner, std::span{&storage_declaration, 1});
  const auto storage_layout_ref = storage_group_layout.ref();
  granit::pipeline_layout compute_layout;
  if (result.ok())
    result = compute_layout.initialize(owner, std::span{&storage_layout_ref, 1});
  granit::compute_pipeline compute_pipeline;
  if (result.ok()) {
    result = compute_pipeline.initialize(
        owner, {.layout = compute_layout.ref(), .compute_shader = compute.ref()});
  }

  constexpr auto color_usage =
      granit::texture_usage::color_attachment | granit::texture_usage::transfer_source;
  constexpr auto storage_usage =
      granit::texture_usage::storage | granit::texture_usage::transfer_source;
  granit::texture first_color;
  granit::texture second_color;
  granit::texture storage_texture;
  granit::texture_view first_color_view;
  granit::texture_view second_color_view;
  granit::texture_view storage_view;
  if (result.ok())
    result = first_color.initialize(owner, {.format = granit::texture_format::rgba8_unorm,
                                            .usage = color_usage,
                                            .width = 2,
                                            .height = 2});
  if (result.ok())
    result = second_color.initialize(owner, {.format = granit::texture_format::rgba8_unorm,
                                             .usage = color_usage,
                                             .width = 2,
                                             .height = 2});
  if (result.ok())
    result = storage_texture.initialize(owner, {.format = granit::texture_format::rgba8_unorm,
                                                .usage = storage_usage,
                                                .width = 2,
                                                .height = 2});
  if (result.ok())
    result = first_color_view.initialize(owner, first_color.ref());
  if (result.ok())
    result = second_color_view.initialize(owner, second_color.ref());
  if (result.ok())
    result = storage_view.initialize(owner, storage_texture.ref());

  const granit::bind_group_entry storage_entry{.binding = 0, .resource = storage_view.ref()};
  granit::bind_group storage_group;
  if (result.ok())
    result =
        storage_group.initialize(owner, storage_group_layout.ref(), std::span{&storage_entry, 1});

  granit::command_recorder recorder;
  if (result.ok())
    result = recorder.initialize(owner);
  if (result.ok())
    result = recorder.begin();
  if (result.ok())
    result = recorder.bind_compute_pipeline(compute_pipeline);
  if (result.ok())
    result = recorder.bind_compute_group(compute_layout, 0, storage_group);
  if (result.ok())
    result = recorder.dispatch(2, 2);
  constexpr std::array viewports{granit::viewport{0.0F, 0.0F, 2.0F, 2.0F, 0.0F, 1.0F}};
  constexpr std::array scissors{granit::scissor{0, 0, 2, 2}};
  const std::array attachments{
      granit::color_attachment_desc{.view = first_color_view.ref(),
                                    .resolve_view = {},
                                    .load_operation = granit::attachment_load_operation::clear,
                                    .store_operation = granit::attachment_store_operation::store,
                                    .clear_value = {}},
      granit::color_attachment_desc{.view = second_color_view.ref(),
                                    .resolve_view = {},
                                    .load_operation = granit::attachment_load_operation::clear,
                                    .store_operation = granit::attachment_store_operation::store,
                                    .clear_value = {}}};
  if (result.ok())
    result = recorder.begin_rendering({.color_attachments = attachments,
                                       .depth_stencil_attachment = nullptr,
                                       .area = {0, 0, 2, 2},
                                       .layer_count = 1});
  if (result.ok())
    result = recorder.bind_graphics_pipeline(graphics_pipeline);
  if (result.ok())
    result = recorder.set_viewports(0, viewports);
  if (result.ok())
    result = recorder.set_scissors(0, scissors);
  if (result.ok())
    result = recorder.draw(3);
  if (result.ok())
    result = recorder.end_rendering();
  if (result.ok())
    result = recorder.end();
  if (result.ok())
    result = recorder.submit();

  constexpr granit::texture_write_region region{.array_layer_count = 1,
                                                .aspect = granit::texture_aspect::color,
                                                .width = 2,
                                                .height = 2,
                                                .depth = 1};
  if (result.ok())
    result = readback_.create(owner, {.max_result_bytes = 48, .max_operation_count = 3});
  if (result.ok())
    result = readback_.read_texture(storage_texture.ref(), region, storage_result_index_);
  if (result.ok())
    result = readback_.read_texture(first_color.ref(), region, first_color_result_index_);
  if (result.ok())
    result = readback_.read_texture(second_color.ref(), region, second_color_result_index_);
  if (result.ok())
    result = readback_.submit_async(readback_operation_);
  return granit::to_native(result);
}

granit_result pipeline_validation::poll_capability_readback() {
  granit::async_operation_status status;
  auto result = readback_operation_.get_status(status);
  if (result.failed())
    return granit::to_native(result);
  if (!status.complete())
    return GRANIT_ERROR_NOT_READY;
  if (status.state != granit::async_operation_state::succeeded)
    return granit::to_native(status.operation_result);

  const auto validate = [this](const char* label, std::uint32_t index,
                               const std::array<std::byte, 4>& expected) {
    std::array<std::byte, 16> pixels{};
    std::uint64_t required_size{};
    auto value = granit::copy_readback_result(readback_operation_, index, pixels, required_size);
    if (value.failed() || required_size != pixels.size()) {
      std::fprintf(stderr,
                   "GRANIT_DIAGNOSTIC:%s readback failed: result=%d required=%llu actual=%zu\n",
                   label, granit::to_native(value), static_cast<unsigned long long>(required_size),
                   pixels.size());
      return value.failed() ? value : granit::result::internal;
    }
    for (std::size_t offset = 0; offset < pixels.size(); offset += expected.size()) {
      if (!std::equal(expected.begin(), expected.end(), pixels.begin() + offset)) {
        std::fprintf(stderr,
                     "GRANIT_DIAGNOSTIC:%s pixel mismatch at %zu: actual=%u,%u,%u,%u "
                     "expected=%u,%u,%u,%u\n",
                     label, offset / expected.size(), std::to_integer<unsigned>(pixels[offset]),
                     std::to_integer<unsigned>(pixels[offset + 1]),
                     std::to_integer<unsigned>(pixels[offset + 2]),
                     std::to_integer<unsigned>(pixels[offset + 3]),
                     std::to_integer<unsigned>(expected[0]),
                     std::to_integer<unsigned>(expected[1]),
                     std::to_integer<unsigned>(expected[2]),
                     std::to_integer<unsigned>(expected[3]));
        return granit::result::internal;
      }
    }
    return granit::result::success;
  };
  result = validate("storage-texture", storage_result_index_,
                    {std::byte{64}, std::byte{128}, std::byte{191}, std::byte{255}});
  if (result.ok())
    result = validate("mrt-color-0", first_color_result_index_,
                      {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}});
  if (result.ok())
    result = validate("mrt-color-1", second_color_result_index_,
                      {std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255}});
  if (result.ok())
    result = readback_operation_.reset();
  if (result.ok())
    result = readback_.reset_handle();
  return granit::to_native(result);
}

granit_result pipeline_validation::poll() {
  if (phase_ == phase::complete)
    return GRANIT_SUCCESS;
  if (phase_ == phase::readback) {
    const auto result = poll_capability_readback();
    if (result == GRANIT_ERROR_NOT_READY)
      return result;
    if (result != GRANIT_SUCCESS) {
      reset();
      return result;
    }
    phase_ = phase::complete;
    return GRANIT_SUCCESS;
  }
  if (phase_ != phase::warmup)
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
    std::fprintf(stderr,
                 "GRANIT_DIAGNOSTIC:Pipeline 预热结果验收失败：result=%d state=%u "
                 "operation=%d graphics=%d compute=%d\n",
                 result, status.state, status.result, graphics_info.result, compute_info.result);
    reset();
    return result;
  }

  result = granit_async_operation_destroy(renderer_, operation_);
  operation_ = GRANIT_NULL_HANDLE;
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_destroy(renderer_, batch_);
  batch_ = GRANIT_NULL_HANDLE;
  if (result != GRANIT_SUCCESS) {
    std::fprintf(stderr, "GRANIT_DIAGNOSTIC:Pipeline 预热资源清理失败：%d\n", result);
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
    std::fprintf(stderr, "GRANIT_DIAGNOSTIC:同步 Graphics Pipeline 创建失败：%d\n", result);
    reset();
    return result;
  }
  const auto compute_destroy_result = granit_shader_destroy(renderer_, compute_);
  const auto vertex_destroy_result = granit_shader_destroy(renderer_, vertex_);
  const auto layout_destroy_result = granit_pipeline_layout_destroy(renderer_, layout_);
  if (compute_destroy_result != GRANIT_SUCCESS || vertex_destroy_result != GRANIT_SUCCESS ||
      layout_destroy_result != GRANIT_SUCCESS) {
    std::fprintf(stderr,
                 "GRANIT_DIAGNOSTIC:Pipeline 所有权验收清理失败：compute=%d vertex=%d "
                 "layout=%d\n",
                 compute_destroy_result, vertex_destroy_result, layout_destroy_result);
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
    std::fprintf(stderr, "GRANIT_DIAGNOSTIC:Pipeline 生命周期验收失败：%d\n", result);
    reset();
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INTERNAL : result;
  }

  // Linux CI 的软件 WebGPU 在异步 Pipeline 回退时会使外部 Instance 失效；此后继续启动
  // Readback 只能得到内部错误。同步 Pipeline 与生命周期已经在上面验收，像素能力由可用的
  // 浏览器适配器执行，避免把运行器限制误判为后端能力回归。
  if (software_adapter_fallback) {
    std::fprintf(stderr,
                 "GRANIT_DIAGNOSTIC:软件 WebGPU Pipeline 回退后跳过同实例异步像素读回\n");
    phase_ = phase::complete;
    return GRANIT_SUCCESS;
  }

  result = begin_capability_readback();
  if (result != GRANIT_SUCCESS) {
    std::fprintf(stderr, "GRANIT_DIAGNOSTIC:WebGPU 能力读回启动失败：%d\n", result);
    reset();
    return result;
  }
  phase_ = phase::readback;
  return GRANIT_ERROR_NOT_READY;
}

void pipeline_validation::reset() noexcept {
  static_cast<void>(readback_operation_.reset());
  static_cast<void>(readback_.reset_handle());
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
  storage_result_index_ = 0;
  first_color_result_index_ = 0;
  second_color_result_index_ = 0;
  phase_ = phase::idle;
}

} // namespace granit::example::model_viewer::web
