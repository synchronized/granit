// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <granit/pipeline/render_pipeline.h>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/pipeline_warmup.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/swapchain.h>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/texture_asset.h>
#include <granit/renderer/timestamp_query.h>
#include <granit/window.h>

#include "model_viewer/application_core.h"
#include "model_viewer/frame_executor.h"
#include "model_viewer/web/web_input.h"
#include "web/fetch.h"
#include "web/resource_fetch_batch.h"

#include "application.h"

namespace {

granit::example::model_viewer::web::application_options options;

enum class startup_status : int { failed = -1, starting, provider_pending, ready, stopped };

struct web_platform_state {
  granit_window_system window_system{};
  granit_window window{};
  granit_renderer renderer{};
  granit_surface surface{};
  granit_swapchain swapchain{};
  granit_render_pipeline pipeline{};
  granit_shader warmup_vertex{};
  granit_shader warmup_fragment{};
  granit_shader warmup_compute{};
  granit_pipeline_layout warmup_layout{};
  granit_pipeline_warmup_batch warmup_batch{};
  granit_async_operation warmup_operation{};
  std::uint32_t warmup_graphics_index{};
  std::uint32_t warmup_compute_index{};
  std::vector<std::uint32_t> material_warmup_indices;
  startup_status status{startup_status::starting};
  unsigned input_event_count{};
  unsigned applied_input_count{};
  unsigned rendered_frame_count{};
  unsigned resize_count{};
  unsigned quality_generation{};
  unsigned lighting_generation{};
  granit_sample_count sample_count{GRANIT_SAMPLE_COUNT_1};
  unsigned enable_fxaa{1};
  unsigned enable_specular_aa{1};
  unsigned sampler_anisotropy{1};
  std::uint64_t shutdown_live_resource_count{};
  std::uint64_t shutdown_pending_retirement_count{};
  granit::example::model_viewer::web::web_input input;
  std::shared_ptr<granit::example::web::asset_request> asset_request{
      std::make_shared<granit::example::web::asset_request>()};
  granit::example::web::resource_fetch_batch resource_batch;
  granit::example::web::resource_bundle resource_bundle;
  std::string asset_url;
  granit::example::model_viewer::application_core core;
  bool core_renderer_ready{};
  bool resource_batch_started{};
  bool asset_ready{};
  bool upload_active{};
  bool upload_cancel_requested{};
  bool pipeline_validation_started{};
  double renderer_initialization_started_ms{};
  granit::example::model_viewer::gpu_scene_upload_progress upload_progress{};
};

web_platform_state state;

const char*
upload_stage_name(granit::example::model_viewer::gpu_scene_upload_stage stage) noexcept {
  using enum granit::example::model_viewer::gpu_scene_upload_stage;
  switch (stage) {
  case planning:
    return "planning";
  case geometry:
    return "geometry";
  case textures:
    return "textures";
  case samplers:
    return "samplers";
  case meshes:
    return "meshes";
  case materials:
    return "materials";
  }
  return "unknown";
}

bool report_upload_progress(
    const granit::example::model_viewer::gpu_scene_upload_progress& progress, void*) {
  state.upload_progress = progress;
  std::printf("GRANIT_PROGRESS:%s:%u:%u\n", upload_stage_name(progress.stage), progress.completed,
              progress.total);
  // Asyncify 在资源边界恢复浏览器事件循环，使页面可以重绘并接收取消操作。
  emscripten_sleep(0);
  return !state.upload_cancel_requested;
}

const char* load_stage_name(granit::example::gltf::load_stage stage) noexcept {
  using enum granit::example::gltf::load_stage;
  switch (stage) {
  case document:
    return "document";
  case buffers:
    return "buffers";
  case images:
    return "images";
  case materials:
    return "materials";
  case meshes:
    return "meshes";
  case nodes:
    return "nodes";
  }
  return "unknown";
}

bool report_load_progress(const granit::example::gltf::load_progress& progress, void*) {
  std::printf("GRANIT_PROGRESS:%s:%u:%u\n", load_stage_name(progress.stage), progress.completed,
              progress.total);
  emscripten_sleep(0);
  return !state.upload_cancel_requested;
}

void fail(const char* message, granit_result result = GRANIT_ERROR_INITIALIZATION_FAILED) noexcept {
  state.status = startup_status::failed;
  std::fprintf(stderr, "GRANIT_STATUS:failed:%s:%d\n", message, result);
}

std::string selected_model_url() {
  const auto* selected = emscripten_run_script_string(
      "new URLSearchParams(globalThis.location.search).get('model') || ''");
  return selected == nullptr || *selected == '\0' ? std::string{options.default_model_url}
                                                  : std::string{selected};
}

std::string resolve_resource_url(std::string_view model_url, std::string_view resource) {
  if (resource.starts_with("http://") || resource.starts_with("https://") ||
      resource.starts_with('/'))
    return std::string{resource};
  const auto separator = model_url.find_last_of('/');
  if (separator == std::string_view::npos)
    return std::string{resource};
  auto result = std::string{model_url.substr(0, separator + 1)};
  result.append(resource);
  return result;
}

granit_result begin_public_pipeline_validation(granit_texture_format model_color_format) {
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
  granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  desc.code_format = GRANIT_SHADER_CODE_FORMAT_WGSL;
  desc.code = vertex_wgsl;
  desc.code_size = sizeof(vertex_wgsl) - 1;
  auto result = granit_shader_create(state.renderer, &desc, &state.warmup_vertex);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  desc.stage = GRANIT_SHADER_STAGE_FRAGMENT;
  desc.code = fragment_wgsl;
  desc.code_size = sizeof(fragment_wgsl) - 1;
  result = granit_shader_create(state.renderer, &desc, &state.warmup_fragment);
  if (result != GRANIT_SUCCESS)
    return result;
  granit_pipeline_layout_desc layout_desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
  result = granit_pipeline_layout_create(state.renderer, &layout_desc, &state.warmup_layout);
  if (result != GRANIT_SUCCESS)
    return result;
  desc.stage = GRANIT_SHADER_STAGE_COMPUTE;
  desc.code = compute_wgsl;
  desc.code_size = sizeof(compute_wgsl) - 1;
  result = granit_shader_create(state.renderer, &desc, &state.warmup_compute);
  if (result != GRANIT_SUCCESS) {
    std::fprintf(stderr, "GRANIT_DIAGNOSTIC:Compute Shader 创建失败：%d\n", result);
    return result;
  }
  constexpr granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  granit_graphics_pipeline_desc pipeline_desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  pipeline_desc.layout = state.warmup_layout;
  pipeline_desc.vertex_shader = state.warmup_vertex;
  pipeline_desc.fragment_shader = state.warmup_fragment;
  pipeline_desc.color_format_count = 1;
  pipeline_desc.color_formats = &color_format;
  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  result = granit_renderer_get_limits(state.renderer, &limits);
  if (result != GRANIT_SUCCESS ||
      (limits.supported_features & GRANIT_RENDERER_FEATURE_NON_BLOCKING_PIPELINE_WARMUP_BIT) == 0) {
    result = result == GRANIT_SUCCESS ? GRANIT_ERROR_UNSUPPORTED : result;
  }
  if (result == GRANIT_SUCCESS) {
    const granit_pipeline_warmup_batch_desc warmup_desc = GRANIT_PIPELINE_WARMUP_BATCH_DESC_INIT;
    result = granit_pipeline_warmup_batch_create(state.renderer, &warmup_desc, &state.warmup_batch);
  }
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_add_graphics(
        state.renderer, state.warmup_batch, &pipeline_desc, &state.warmup_graphics_index);
  granit_compute_pipeline_desc compute_desc = GRANIT_COMPUTE_PIPELINE_DESC_INIT;
  compute_desc.layout = state.warmup_layout;
  compute_desc.compute_shader = state.warmup_compute;
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_add_compute(state.renderer, state.warmup_batch,
                                                      &compute_desc, &state.warmup_compute_index);
  if (result == GRANIT_SUCCESS) {
    result = granit::to_native(state.core.scene_gpu().add_pipeline_warmups(
        granit::pipeline_warmup_batch_ref::from_native(state.warmup_batch),
        static_cast<granit::texture_format>(model_color_format),
        static_cast<granit::sample_count>(state.sample_count), state.material_warmup_indices));
  }
  if (result != GRANIT_SUCCESS)
    std::fprintf(stderr, "GRANIT_DIAGNOSTIC:Pipeline 预热批次构建失败：%d\n", result);
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_submit_async(state.renderer, state.warmup_batch,
                                                       &state.warmup_operation);
  if (result == GRANIT_SUCCESS)
    std::printf("GRANIT_PROGRESS:pipelines:0:%zu\n", state.material_warmup_indices.size() + 2);
  state.pipeline_validation_started = result == GRANIT_SUCCESS;
  return result;
}

void cleanup_public_pipeline_validation() noexcept {
  if (state.warmup_operation != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_async_operation_destroy(state.renderer, state.warmup_operation));
  if (state.warmup_batch != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_pipeline_warmup_batch_destroy(state.renderer, state.warmup_batch));
  if (state.warmup_layout != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_pipeline_layout_destroy(state.renderer, state.warmup_layout));
  if (state.warmup_compute != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_shader_destroy(state.renderer, state.warmup_compute));
  if (state.warmup_fragment != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_shader_destroy(state.renderer, state.warmup_fragment));
  if (state.warmup_vertex != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_shader_destroy(state.renderer, state.warmup_vertex));
  state.warmup_operation = GRANIT_NULL_HANDLE;
  state.warmup_batch = GRANIT_NULL_HANDLE;
  state.warmup_layout = GRANIT_NULL_HANDLE;
  state.warmup_compute = GRANIT_NULL_HANDLE;
  state.warmup_fragment = GRANIT_NULL_HANDLE;
  state.warmup_vertex = GRANIT_NULL_HANDLE;
}

granit_result poll_public_pipeline_validation() {
  granit_async_operation_status warmup_status = GRANIT_ASYNC_OPERATION_STATUS_INIT;
  auto result =
      granit_async_operation_get_status(state.renderer, state.warmup_operation, &warmup_status);
  if (result == GRANIT_SUCCESS && warmup_status.state == GRANIT_ASYNC_OPERATION_STATE_RUNNING)
    return GRANIT_ERROR_NOT_READY;
  granit_pipeline_warmup_result_info warmup_info = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;
  granit_pipeline_warmup_result_info compute_warmup_info = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;
  bool software_adapter_fallback{};
  const auto accept_software_adapter_failure = [&software_adapter_fallback](granit_result value) {
    if (value == GRANIT_ERROR_INTERNAL) {
      software_adapter_fallback = true;
      return GRANIT_SUCCESS;
    }
    return value;
  };
  if (result == GRANIT_SUCCESS && warmup_status.state == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED) {
    result = granit_pipeline_warmup_operation_get_result(state.renderer, state.warmup_operation,
                                                         state.warmup_graphics_index, &warmup_info);
    if (result == GRANIT_SUCCESS)
      result = granit_pipeline_warmup_operation_get_result(
          state.renderer, state.warmup_operation, state.warmup_compute_index, &compute_warmup_info);
    if (result == GRANIT_SUCCESS)
      result = accept_software_adapter_failure(warmup_info.result);
    if (result == GRANIT_SUCCESS)
      result = accept_software_adapter_failure(compute_warmup_info.result);
    for (const auto index : state.material_warmup_indices) {
      granit_pipeline_warmup_result_info material_info = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;
      if (result == GRANIT_SUCCESS)
        result = granit_pipeline_warmup_operation_get_result(state.renderer, state.warmup_operation,
                                                             index, &material_info);
      if (result == GRANIT_SUCCESS)
        result = accept_software_adapter_failure(material_info.result);
    }
  } else if (result == GRANIT_SUCCESS) {
    result = warmup_status.result == GRANIT_ERROR_NOT_READY ? GRANIT_ERROR_NOT_READY
                                                            : warmup_status.result;
  }
  if (result == GRANIT_SUCCESS)
    std::printf("GRANIT_PROGRESS:pipelines:%zu:%zu\n", state.material_warmup_indices.size() + 2,
                state.material_warmup_indices.size() + 2);
  if (software_adapter_fallback) {
    constexpr char message[] =
        "GRANIT_DIAGNOSTIC:软件 WebGPU 适配器异步编译失败，回退到按需同步创建管线";
    std::fprintf(stderr, "%s\n", message);
  }
  if (result != GRANIT_SUCCESS) {
    cleanup_public_pipeline_validation();
    return result;
  }
  result = granit_async_operation_destroy(state.renderer, state.warmup_operation);
  state.warmup_operation = GRANIT_NULL_HANDLE;
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_destroy(state.renderer, state.warmup_batch);
  state.warmup_batch = GRANIT_NULL_HANDLE;
  if (result != GRANIT_SUCCESS) {
    cleanup_public_pipeline_validation();
    return result;
  }
  granit_graphics_pipeline pipeline{};
  constexpr granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  granit_graphics_pipeline_desc pipeline_desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  pipeline_desc.layout = state.warmup_layout;
  pipeline_desc.vertex_shader = state.warmup_vertex;
  pipeline_desc.fragment_shader = state.warmup_fragment;
  pipeline_desc.color_format_count = 1;
  pipeline_desc.color_formats = &color_format;
  result = granit_graphics_pipeline_create(state.renderer, &pipeline_desc, &pipeline);
  if (result != GRANIT_SUCCESS) {
    cleanup_public_pipeline_validation();
    return result;
  }
  if (granit_shader_destroy(state.renderer, state.warmup_compute) != GRANIT_SUCCESS ||
      granit_shader_destroy(state.renderer, state.warmup_vertex) != GRANIT_SUCCESS ||
      granit_pipeline_layout_destroy(state.renderer, state.warmup_layout) != GRANIT_SUCCESS) {
    static_cast<void>(granit_graphics_pipeline_destroy(state.renderer, pipeline));
    cleanup_public_pipeline_validation();
    return GRANIT_ERROR_INTERNAL;
  }
  state.warmup_compute = GRANIT_NULL_HANDLE;
  state.warmup_vertex = GRANIT_NULL_HANDLE;
  state.warmup_layout = GRANIT_NULL_HANDLE;
  result = granit_graphics_pipeline_destroy(state.renderer, pipeline);
  if (result == GRANIT_SUCCESS) {
    result = granit_shader_destroy(state.renderer, state.warmup_fragment);
    if (result == GRANIT_SUCCESS)
      state.warmup_fragment = GRANIT_NULL_HANDLE;
  }
  if (result != GRANIT_SUCCESS ||
      granit_shader_destroy(state.renderer, state.warmup_vertex) != GRANIT_ERROR_INVALID_HANDLE) {
    cleanup_public_pipeline_validation();
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INTERNAL : result;
  }
  return GRANIT_SUCCESS;
}

void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category, const char* message,
              std::uint32_t message_length, void*) noexcept {
  auto* stream = severity == GRANIT_DIAGNOSTIC_SEVERITY_INFO ? stdout : stderr;
  std::fprintf(stream, "GRANIT_DIAGNOSTIC:%.*s\n", static_cast<int>(message_length), message);
}

granit_result process_window_events() {
  auto result = granit_window_system_process_events(state.window_system);
  if (result != GRANIT_SUCCESS)
    return result;

  granit_window_event window_event = GRANIT_WINDOW_EVENT_INIT;
  while ((result = granit_window_poll_event(state.window_system, &window_event)) ==
         GRANIT_SUCCESS) {
    if (window_event.type == GRANIT_WINDOW_EVENT_FOCUS_CHANGED) {
      ++state.input_event_count;
      state.input.focus_changed(window_event.data.focus.focused != 0);
    }
    window_event = GRANIT_WINDOW_EVENT_INIT;
  }
  if (result != GRANIT_ERROR_NOT_READY)
    return result;

  granit_input_event input_event = GRANIT_INPUT_EVENT_INIT;
  while ((result = granit_window_poll_input_event(state.window_system, &input_event)) ==
         GRANIT_SUCCESS) {
    ++state.input_event_count;
    switch (input_event.type) {
    case GRANIT_INPUT_EVENT_KEY: {
      using granit::example::model_viewer::web::shortcut_key;
      auto key = shortcut_key::other;
      if (input_event.data.key.physical_key == GRANIT_PHYSICAL_KEY_F)
        key = shortcut_key::focus;
      else if (input_event.data.key.logical_key == GRANIT_LOGICAL_KEY_HOME)
        key = shortcut_key::home;
      if (input_event.data.key.action != GRANIT_KEY_ACTION_RELEASED) {
        state.input.key_pressed(key, input_event.data.key.action == GRANIT_KEY_ACTION_REPEATED);
      }
      break;
    }
    case GRANIT_INPUT_EVENT_POINTER_ENTERED:
      state.input.pointer_presence_changed(true);
      break;
    case GRANIT_INPUT_EVENT_POINTER_LEFT:
      state.input.pointer_presence_changed(false);
      break;
    case GRANIT_INPUT_EVENT_POINTER_MOVED:
      state.input.pointer_motion(input_event.data.pointer_moved.delta_x,
                                 input_event.data.pointer_moved.delta_y);
      break;
    case GRANIT_INPUT_EVENT_POINTER_BUTTON: {
      using granit::example::model_viewer::web::pointer_button;
      auto button = pointer_button::primary;
      if (input_event.data.pointer_button.button == GRANIT_POINTER_MIDDLE_BIT)
        button = pointer_button::middle;
      else if (input_event.data.pointer_button.button == GRANIT_POINTER_SECONDARY_BIT)
        button = pointer_button::secondary;
      state.input.pointer_button_changed(button, input_event.data.pointer_button.pressed != 0);
      break;
    }
    case GRANIT_INPUT_EVENT_POINTER_WHEEL:
      state.input.wheel(input_event.data.pointer_wheel.delta_y);
      break;
    default:
      break;
    }
    input_event = GRANIT_INPUT_EVENT_INIT;
  }
  return result == GRANIT_ERROR_NOT_READY ? GRANIT_SUCCESS : result;
}

granit_result create_window_resources() {
  double width = 0.0;
  double height = 0.0;
  if (emscripten_get_element_css_size("#canvas", &width, &height) != EMSCRIPTEN_RESULT_SUCCESS ||
      width < 1.0 || height < 1.0) {
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }
  granit_window_system_desc system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  system_desc.backend = GRANIT_WINDOW_BACKEND_EMSCRIPTEN;
  auto result = granit_window_system_create(&system_desc, &state.window_system);
  if (result != GRANIT_SUCCESS)
    return result;
  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.width = static_cast<std::uint32_t>(std::lround(width));
  window_desc.height = static_cast<std::uint32_t>(std::lround(height));
  window_desc.flags =
      GRANIT_WINDOW_VISIBLE_BIT | GRANIT_WINDOW_RESIZABLE_BIT | GRANIT_WINDOW_HIGH_DPI_BIT;
  result = granit_window_create(state.window_system, &window_desc, &state.window);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_window_system_destroy(state.window_system));
    state.window_system = GRANIT_NULL_HANDLE;
    return result;
  }
  state.input.focus_changed(true);
  return GRANIT_SUCCESS;
}

granit_result create_presentation_resources() {
  granit_window_state window_state = GRANIT_WINDOW_STATE_INIT;
  auto result = granit_window_get_state(state.window_system, state.window, &window_state);
  if (result != GRANIT_SUCCESS)
    return result;
  result = granit_window_create_surface(state.window_system, state.window, state.renderer,
                                        &state.surface);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  granit_swapchain_desc swapchain_desc = GRANIT_SWAPCHAIN_DESC_INIT;
  swapchain_desc.width = window_state.framebuffer_width;
  swapchain_desc.height = window_state.framebuffer_height;
  swapchain_desc.minimum_image_count = 2;
  result =
      granit_swapchain_create(state.renderer, state.surface, &swapchain_desc, &state.swapchain);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;
  result = granit_swapchain_get_info(state.renderer, state.swapchain, &info);
  if (result != GRANIT_SUCCESS || info.width == 0 || info.height == 0 || info.image_count == 0) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  if (options.presentation_ready != nullptr)
    return options.presentation_ready(state.renderer, state.swapchain, info);
  return GRANIT_SUCCESS;
}

granit_result resize_swapchain_if_needed() {
  granit_window_state window_state = GRANIT_WINDOW_STATE_INIT;
  auto result = granit_window_get_state(state.window_system, state.window, &window_state);
  if (result != GRANIT_SUCCESS)
    return result;
  granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;
  result = granit_swapchain_get_info(state.renderer, state.swapchain, &info);
  if (result != GRANIT_SUCCESS || (info.width == window_state.framebuffer_width &&
                                   info.height == window_state.framebuffer_height)) {
    return result;
  }
  result = granit_swapchain_destroy(state.renderer, state.swapchain);
  if (result != GRANIT_SUCCESS)
    return result;
  state.swapchain = GRANIT_NULL_HANDLE;
  granit_swapchain_desc desc = GRANIT_SWAPCHAIN_DESC_INIT;
  desc.width = window_state.framebuffer_width;
  desc.height = window_state.framebuffer_height;
  desc.minimum_image_count = 2;
  result = granit_swapchain_create(state.renderer, state.surface, &desc, &state.swapchain);
  if (result == GRANIT_SUCCESS)
    ++state.resize_count;
  return result;
}

struct web_frame_execution_context {
  const granit_swapchain_info* swapchain_info{};
};

granit::result execute_web_frame(granit::example::model_viewer::frame_packet&& packet,
                                 granit::example::model_viewer::frame_execution_result& output,
                                 void* user_data) {
  const auto& context = *static_cast<web_frame_execution_context*>(user_data);
  granit_frame frame{};
  std::uint32_t image_index{};
  std::uint32_t needs_recreate{};
  auto result = granit_swapchain_acquire(state.renderer, state.swapchain, &frame, &image_index,
                                         &needs_recreate);
  output.needs_recreate = needs_recreate != 0;
  granit_texture backbuffer{};
  granit_texture_view backbuffer_view{};
  if (result == GRANIT_SUCCESS) {
    result = granit_swapchain_get_backbuffer(state.renderer, state.swapchain, image_index,
                                             &backbuffer, &backbuffer_view);
  }
  if (result == GRANIT_SUCCESS) {
    const auto render = packet.render_desc(backbuffer_view, context.swapchain_info->format, frame);
    result = granit_render_pipeline_render(state.renderer, state.pipeline, &render);
  }
  if (result != GRANIT_SUCCESS) {
    if (frame != GRANIT_NULL_HANDLE) {
      static_cast<void>(
          granit_frame_cancel(state.renderer, state.swapchain, frame, &needs_recreate));
      output.needs_recreate = output.needs_recreate || needs_recreate != 0;
    }
    return granit::from_native(result);
  }
  result = granit_swapchain_present(state.renderer, state.swapchain, frame, &needs_recreate);
  output.needs_recreate = output.needs_recreate || needs_recreate != 0;
  return granit::from_native(result);
}

granit_result render_model_viewer_frame() {
  auto result = resize_swapchain_if_needed();
  if (result != GRANIT_SUCCESS)
    return result;
  granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;
  result = granit_swapchain_get_info(state.renderer, state.swapchain, &info);
  granit::example::model_viewer::frame_packet output;
  granit::example::model_viewer::application_tick_input input;
  input.input = state.input.finish(false, false);
  if (input.input.pointer_delta_x != 0.0F || input.input.pointer_delta_y != 0.0F ||
      input.input.wheel_delta != 0.0F || input.input.focus_requested || input.input.home_requested)
    ++state.applied_input_count;
  state.input.begin_frame();
  input.width = info.width;
  input.height = info.height;
  if (result == GRANIT_SUCCESS)
    result = granit::to_native(state.core.tick(input, output));
  if (result == GRANIT_SUCCESS) {
    web_frame_execution_context execution_context{&info};
    granit::example::model_viewer::inline_frame_executor executor(execute_web_frame,
                                                                  &execution_context);
    granit::example::model_viewer::frame_execution_result execution;
    result = granit::to_native(executor.submit(std::move(output), execution));
  }
  if (result == GRANIT_SUCCESS)
    ++state.rendered_frame_count;
  return result;
}

granit_result configure_render_quality(granit_sample_count sample_count, unsigned enable_fxaa,
                                       unsigned enable_specular_aa, unsigned sampler_anisotropy) {
  if (state.status != startup_status::ready || state.renderer == GRANIT_NULL_HANDLE ||
      state.pipeline == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_NOT_READY;
  if ((sample_count != GRANIT_SAMPLE_COUNT_1 && sample_count != GRANIT_SAMPLE_COUNT_4) ||
      enable_fxaa > 1 || enable_specular_aa > 1 || sampler_anisotropy == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  auto result = granit_renderer_get_limits(state.renderer, &limits);
  if (result != GRANIT_SUCCESS)
    return result;
  if ((limits.framebuffer_sample_counts & sample_count) == 0 ||
      static_cast<float>(sampler_anisotropy) > limits.max_sampler_anisotropy)
    return GRANIT_ERROR_UNSUPPORTED;

  granit_render_pipeline_desc desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  desc.sample_count = sample_count;
  desc.enable_fxaa = enable_fxaa;
  desc.enable_specular_aa = enable_specular_aa;
  granit_render_pipeline replacement{};
  result = granit_render_pipeline_create(state.renderer, &desc, &replacement);
  if (result != GRANIT_SUCCESS)
    return result;
  if (sampler_anisotropy != state.sampler_anisotropy) {
    result = granit::to_native(
        state.core.reupload_scene(state.renderer, static_cast<float>(sampler_anisotropy)));
    if (result != GRANIT_SUCCESS) {
      static_cast<void>(granit_render_pipeline_destroy(state.renderer, replacement));
      return result;
    }
  }
  result = granit_render_pipeline_destroy(state.renderer, state.pipeline);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_render_pipeline_destroy(state.renderer, replacement));
    return result;
  }
  state.pipeline = replacement;
  state.sample_count = sample_count;
  state.enable_fxaa = enable_fxaa;
  state.enable_specular_aa = enable_specular_aa;
  state.sampler_anisotropy = sampler_anisotropy;
  ++state.quality_generation;
  return GRANIT_SUCCESS;
}

granit_result configure_lighting(float exposure_ev, float environment_intensity,
                                 float key_light_intensity) {
  if (state.status != startup_status::ready || !state.asset_ready)
    return GRANIT_ERROR_NOT_READY;

  granit::example::model_viewer::viewer_change change;
  change.exposure_ev = exposure_ev;
  change.environment_intensity = environment_intensity;
  auto light = state.core.state().directional_light();
  light.radiance = {key_light_intensity, key_light_intensity, key_light_intensity};
  change.directional_light = light;
  if (state.core.state().apply(state.core.cpu_scene(), change) !=
      granit::example::model_viewer::viewer_state_error::none)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  ++state.lighting_generation;
  return GRANIT_SUCCESS;
}

void tick(void*) noexcept {
  if (state.status == startup_status::failed) {
    return;
  }
  const auto window_result = process_window_events();
  if (window_result != GRANIT_SUCCESS) {
    fail("window-events", window_result);
    return;
  }
  if (state.status == startup_status::ready) {
    const auto result = render_model_viewer_frame();
    if (result != GRANIT_SUCCESS)
      fail("model-viewer-frame", result);
    return;
  }
  if (state.status != startup_status::provider_pending)
    return;
  constexpr double renderer_initialization_timeout_ms = 30000.0;
  if (emscripten_get_now() - state.renderer_initialization_started_ms >
      renderer_initialization_timeout_ms) {
    fail("renderer-timeout", GRANIT_ERROR_NOT_READY);
    return;
  }
  const auto process_result = granit_renderer_process_events(state.renderer);
  if (process_result != GRANIT_SUCCESS) {
    fail("provider-events", process_result);
    return;
  }

  granit_renderer_status renderer_status = GRANIT_RENDERER_STATUS_INIT;
  const auto status_result = granit_renderer_get_status(state.renderer, &renderer_status);
  if (status_result != GRANIT_SUCCESS) {
    fail("renderer-status", status_result);
    return;
  }
  if (renderer_status.state == GRANIT_RENDERER_STATE_FAILED ||
      renderer_status.state == GRANIT_RENDERER_STATE_DEVICE_LOST) {
    fail("provider-terminal", renderer_status.failure_result);
    return;
  }
  if (renderer_status.state != GRANIT_RENDERER_STATE_READY) {
    return;
  }
  if (!state.core_renderer_ready) {
    const auto result = state.core.renderer_ready();
    if (result != granit::result::success) {
      fail("core-renderer-ready", granit::to_native(result));
      return;
    }
    state.core_renderer_ready = true;
  }
  if (state.asset_request->status() == granit::example::web::asset_request_status::failed) {
    fail("asset-fetch");
    return;
  }
  if (state.asset_request->status() != granit::example::web::asset_request_status::ready) {
    return;
  }

  try {
    if (!state.resource_batch_started) {
      std::vector<std::string> resources;
      const auto discovery = granit::example::gltf::discover_external_resources(
          state.asset_request->bytes(), resources);
      if (!discovery) {
        fail("asset-discovery", GRANIT_ERROR_INVALID_ARGUMENT);
        return;
      }
      for (const auto& resource : resources) {
        if (!state.resource_batch.add(resource, resolve_resource_url(state.asset_url, resource))) {
          fail("asset-batch-add", GRANIT_ERROR_INVALID_ARGUMENT);
          return;
        }
      }
      for (const auto& entry : state.resource_batch.entries()) {
        if (!granit::example::web::start_fetch(entry.request, entry.url)) {
          fail("asset-resource-fetch-start");
          return;
        }
      }
      state.resource_batch_started = true;
    }

    const auto batch_status = state.resource_batch.status();
    if (batch_status == granit::example::web::resource_fetch_batch_status::failed) {
      fail("asset-resource-fetch");
      return;
    }
    if (batch_status != granit::example::web::resource_fetch_batch_status::ready)
      return;
    if (!state.asset_ready) {
      if (!state.resource_batch.commit(state.resource_bundle)) {
        fail("asset-bundle-commit", GRANIT_ERROR_INTERNAL);
        return;
      }
      state.upload_active = true;
      state.upload_cancel_requested = false;
      auto result = state.core.load_asset(state.asset_request->bytes(), &state.resource_bundle,
                                          report_load_progress, nullptr);
      if (result != granit::result::success) {
        state.upload_active = false;
        fail("asset-load", granit::to_native(result));
        return;
      }
      result = state.core.upload(state.renderer, {}, 8.0F, report_upload_progress, nullptr);
      state.upload_active = false;
      if (result != granit::result::success) {
        fail("asset-upload", granit::to_native(result));
        return;
      }
      if (state.core.phase() != granit::example::model_viewer::application_phase::ready) {
        fail("asset-core-phase", GRANIT_ERROR_INTERNAL);
        return;
      }
      state.asset_ready = true;
    }
  } catch (const std::bad_alloc&) {
    fail("asset-allocation", GRANIT_ERROR_OUT_OF_MEMORY);
    return;
  } catch (...) {
    fail("asset-exception", GRANIT_ERROR_INTERNAL);
    return;
  }

  if (!state.pipeline_validation_started) {
    granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
    const auto limits_result = granit_renderer_get_limits(state.renderer, &limits);
    if (limits_result != GRANIT_SUCCESS || limits.uniform_buffer_offset_alignment == 0 ||
        limits.max_uniform_buffer_binding_size == 0) {
      fail("renderer-limits",
           limits_result == GRANIT_SUCCESS ? GRANIT_ERROR_INTERNAL : limits_result);
      return;
    }
    if (options.renderer_ready != nullptr) {
      const auto validation_result = options.renderer_ready(state.renderer, limits);
      if (validation_result != GRANIT_SUCCESS) {
        fail("renderer-validation", validation_result);
        return;
      }
    }
  }
  try {
    if (state.surface == GRANIT_NULL_HANDLE) {
      const auto create_result = create_presentation_resources();
      if (create_result != GRANIT_SUCCESS) {
        fail("presentation-create", create_result);
        return;
      }
    }
    granit_swapchain_info swapchain_info = GRANIT_SWAPCHAIN_INFO_INIT;
    auto result = granit_swapchain_get_info(state.renderer, state.swapchain, &swapchain_info);
    if (result != GRANIT_SUCCESS) {
      fail("presentation-info", result);
      return;
    }
    if (!state.pipeline_validation_started) {
      state.upload_active = true;
      state.upload_cancel_requested = false;
      const auto begin_result = begin_public_pipeline_validation(swapchain_info.format);
      if (begin_result != GRANIT_SUCCESS) {
        state.upload_active = false;
        cleanup_public_pipeline_validation();
        fail("renderer-pipeline", begin_result);
        return;
      }
    }
    const auto pipeline_result = poll_public_pipeline_validation();
    if (pipeline_result == GRANIT_ERROR_NOT_READY)
      return;
    state.upload_active = false;
    if (pipeline_result != GRANIT_SUCCESS) {
      cleanup_public_pipeline_validation();
      fail("renderer-pipeline", pipeline_result);
      return;
    }
    const granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
    const auto pipeline_create_result =
        granit_render_pipeline_create(state.renderer, &pipeline_desc, &state.pipeline);
    if (pipeline_create_result != GRANIT_SUCCESS) {
      fail("pipeline-create", pipeline_create_result);
      return;
    }
    const auto render_result = render_model_viewer_frame();
    if (render_result != GRANIT_SUCCESS) {
      static_cast<void>(granit_render_pipeline_destroy(state.renderer, state.pipeline));
      state.pipeline = GRANIT_NULL_HANDLE;
      fail("model-viewer-render", render_result);
      return;
    }
  } catch (const std::bad_alloc&) {
    fail("presentation-allocation", GRANIT_ERROR_OUT_OF_MEMORY);
    return;
  } catch (...) {
    fail("presentation-exception", GRANIT_ERROR_INTERNAL);
    return;
  }
  state.status = startup_status::ready;
  std::puts("GRANIT_STATUS:ready");
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_platform_status() noexcept {
  return static_cast<int>(state.status);
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_input_event_count() noexcept {
  return state.input_event_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_rendered_frame_count() noexcept {
  return state.rendered_frame_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_applied_input_count() noexcept {
  return state.applied_input_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_resize_count() noexcept {
  return state.resize_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE int
granit_web_configure_render_quality(unsigned sample_count, unsigned enable_fxaa,
                                    unsigned enable_specular_aa,
                                    unsigned sampler_anisotropy) noexcept {
  try {
    return configure_render_quality(static_cast<granit_sample_count>(sample_count), enable_fxaa,
                                    enable_specular_aa, sampler_anisotropy);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_quality_generation() noexcept {
  return state.quality_generation;
}

extern "C" EMSCRIPTEN_KEEPALIVE int
granit_web_configure_lighting(float exposure_ev, float environment_intensity,
                              float key_light_intensity) noexcept {
  try {
    return configure_lighting(exposure_ev, environment_intensity, key_light_intensity);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_lighting_generation() noexcept {
  return state.lighting_generation;
}

extern "C" EMSCRIPTEN_KEEPALIVE float granit_web_exposure_ev() noexcept {
  return state.core.state().exposure_ev();
}

extern "C" EMSCRIPTEN_KEEPALIVE float granit_web_environment_intensity() noexcept {
  return state.core.state().environment_intensity();
}

extern "C" EMSCRIPTEN_KEEPALIVE float granit_web_key_light_intensity() noexcept {
  return state.core.state().directional_light().radiance.x;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_max_sampler_anisotropy() noexcept {
  if (state.renderer == GRANIT_NULL_HANDLE)
    return 0;
  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  if (granit_renderer_get_limits(state.renderer, &limits) != GRANIT_SUCCESS)
    return 0;
  return static_cast<unsigned>(limits.max_sampler_anisotropy);
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned long long
granit_web_shutdown_live_resource_count() noexcept {
  return state.shutdown_live_resource_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned long long
granit_web_shutdown_pending_retirement_count() noexcept {
  return state.shutdown_pending_retirement_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_shutdown() noexcept {
  if (state.status == startup_status::stopped)
    return GRANIT_SUCCESS;
  emscripten_cancel_main_loop();
  state.asset_request->cancel();
  for (const auto& entry : state.resource_batch.entries())
    entry.request->cancel();
  state.resource_batch.clear();
  granit::example::web::resource_bundle empty_bundle;
  state.resource_bundle.swap(empty_bundle);
  state.core.reset();

  auto first_error = GRANIT_SUCCESS;
  const auto capture = [&](granit_result result) {
    if (first_error == GRANIT_SUCCESS && result != GRANIT_SUCCESS)
      first_error = result;
  };
  if (state.pipeline_validation_started)
    cleanup_public_pipeline_validation();
  if (state.pipeline != GRANIT_NULL_HANDLE) {
    capture(granit_render_pipeline_destroy(state.renderer, state.pipeline));
    state.pipeline = GRANIT_NULL_HANDLE;
  }
  if (state.swapchain != GRANIT_NULL_HANDLE) {
    capture(granit_swapchain_destroy(state.renderer, state.swapchain));
    state.swapchain = GRANIT_NULL_HANDLE;
  }
  if (state.surface != GRANIT_NULL_HANDLE) {
    capture(granit_surface_destroy(state.renderer, state.surface));
    state.surface = GRANIT_NULL_HANDLE;
  }
  if (state.window != GRANIT_NULL_HANDLE) {
    capture(granit_window_destroy(state.window_system, state.window));
    state.window = GRANIT_NULL_HANDLE;
  }
  if (state.window_system != GRANIT_NULL_HANDLE) {
    capture(granit_window_system_destroy(state.window_system));
    state.window_system = GRANIT_NULL_HANDLE;
  }
  if (state.renderer != GRANIT_NULL_HANDLE) {
    granit_renderer_resource_stats stats = GRANIT_RENDERER_RESOURCE_STATS_INIT;
    const auto stats_result = granit_renderer_get_resource_stats(state.renderer, &stats);
    capture(stats_result);
    if (stats_result == GRANIT_SUCCESS) {
      state.shutdown_live_resource_count = stats.total_live_count;
      state.shutdown_pending_retirement_count = stats.pending_retirement_count;
      if (stats.total_live_count != 0)
        capture(GRANIT_ERROR_INTERNAL);
    }
    capture(granit_renderer_destroy(state.renderer));
    state.renderer = GRANIT_NULL_HANDLE;
  }
  state.status = startup_status::stopped;
  return first_error;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_asset_status() noexcept {
  if (state.status == startup_status::failed)
    return 3;
  return state.asset_ready ? 2U : 1U;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_upload_stage() noexcept {
  return static_cast<unsigned>(state.upload_progress.stage);
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_upload_completed() noexcept {
  return state.upload_progress.completed;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_upload_total() noexcept {
  return state.upload_progress.total;
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_cancel_loading() noexcept {
  if (!state.upload_active)
    return GRANIT_ERROR_NOT_READY;
  state.upload_cancel_requested = true;
  return GRANIT_SUCCESS;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_renderer_state() noexcept {
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  return granit_renderer_get_status(state.renderer, &status) == GRANIT_SUCCESS ? status.state : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_renderer_failure_result() noexcept {
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  return granit_renderer_get_status(state.renderer, &status) == GRANIT_SUCCESS
             ? status.failure_result
             : GRANIT_ERROR_INVALID_HANDLE;
}

int granit::example::model_viewer::web::run_application(const application_options& configuration) {
  options = configuration;
  const auto window_result = create_window_resources();
  if (window_result != GRANIT_SUCCESS) {
    fail("window-create", window_result);
    return 1;
  }
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.presentation_mode = GRANIT_PRESENTATION_ENABLED;
  desc.diagnostic_callback = diagnose;
  const auto result = granit_renderer_create(&desc, &state.renderer);
  if (result != GRANIT_SUCCESS) {
    fail("provider-open", result);
    static_cast<void>(granit_window_destroy(state.window_system, state.window));
    static_cast<void>(granit_window_system_destroy(state.window_system));
    state.window = GRANIT_NULL_HANDLE;
    state.window_system = GRANIT_NULL_HANDLE;
    return 1;
  }
  const auto core_result = state.core.begin_renderer();
  if (core_result != granit::result::success) {
    fail("core-renderer-begin", granit::to_native(core_result));
    return 1;
  }
  state.asset_url = selected_model_url();
  if (!granit::example::web::start_fetch(state.asset_request, state.asset_url)) {
    fail("asset-fetch-start");
    return 1;
  }
  state.renderer_initialization_started_ms = emscripten_get_now();
  state.status = startup_status::provider_pending;
  emscripten_set_main_loop_arg(tick, &state, 0, EM_FALSE);
  return 0;
}
