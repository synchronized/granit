// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
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
#include <granit/renderer/renderer.h>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/swapchain.h>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/texture_asset.h>
#include <granit/renderer/timestamp_query.h>
#include <granit/window.h>

#include "application/application_host.h"
#include "model_viewer/application_core.h"
#include "model_viewer/render_task_executor.h"
#include "model_viewer/model_viewer_runtime.h"
#include "model_viewer/viewer_input_accumulator.h"

#include "application.h"
#include "pipeline_validation.h"
#include "runtime_control.h"

namespace {

granit::example::model_viewer::web::application_options options;

enum class startup_status : int { failed = -1, starting, provider_pending, ready, stopped };

struct web_platform_state {
  granit_renderer renderer{};
  granit_surface surface{};
  granit_swapchain swapchain{};
  granit_swapchain_info swapchain_info = GRANIT_SWAPCHAIN_INFO_INIT;
  granit_render_pipeline pipeline{};
  granit::example::model_viewer::inline_render_task_executor executor;
  granit::example::model_viewer::web::pipeline_validation pipeline_validation;
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
  granit_result shutdown_result{GRANIT_SUCCESS};
  bool shutdown_complete{};
  granit::example::model_viewer::viewer_input_accumulator input;
  granit::example::model_viewer::model_loading_session model_loading;
  std::string asset_url;
  granit::example::assets::asset_mount asset_mount;
  std::string asset_path;
  granit::example::model_viewer::application_core core;
  granit::example::model_viewer::model_viewer_runtime runtime{core, model_loading};
  bool core_renderer_ready{};
  bool asset_ready{};
  bool upload_active{};
  bool upload_cancel_requested{};
  double renderer_initialization_started_ms{};
  granit::example::model_viewer::gpu_scene_upload_progress upload_progress{};
};

web_platform_state state;

class web_application_host final : public granit::example::application_host {
public:
  [[nodiscard]] granit_window_system window_system_handle() noexcept {
    return app_window_system().native_handle();
  }
  [[nodiscard]] granit_window window_handle() noexcept { return app_window().native_handle(); }
  void request_shutdown() noexcept { request_stop(); }

private:
  [[nodiscard]] granit::result on_host_initialize() noexcept override;
  [[nodiscard]] granit::result on_host_update(float delta_seconds,
                                              granit::window_loop_action& action) noexcept override;
  [[nodiscard]] granit::result
  on_host_window_event(const granit::window_event& event) noexcept override;
  [[nodiscard]] granit::result
  on_host_input_event(const granit::input_event& event) noexcept override;
  void on_host_shutdown(granit::result reason) noexcept override;
};

web_application_host web_host;

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

const char* load_stage_name(granit::example::gltf::import_stage stage) noexcept {
  using enum granit::example::gltf::import_stage;
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

bool report_load_progress(const granit::example::gltf::import_progress& progress, void*) {
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

void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category, const char* message,
              std::uint32_t message_length, void*) noexcept {
  auto* stream = severity == GRANIT_DIAGNOSTIC_SEVERITY_INFO ? stdout : stderr;
  std::fprintf(stream, "GRANIT_DIAGNOSTIC:%.*s\n", static_cast<int>(message_length), message);
}

granit_result create_presentation_resources() {
  granit_window_state window_state = GRANIT_WINDOW_STATE_INIT;
  auto result = granit_window_get_state(web_host.window_system_handle(), web_host.window_handle(),
                                        &window_state);
  if (result != GRANIT_SUCCESS)
    return result;
  result = granit_window_create_surface(web_host.window_system_handle(), web_host.window_handle(),
                                        state.renderer, &state.surface);
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
  result = granit_swapchain_get_info(state.renderer, state.swapchain, &state.swapchain_info);
  if (result != GRANIT_SUCCESS || state.swapchain_info.width == 0 ||
      state.swapchain_info.height == 0 || state.swapchain_info.image_count == 0) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  if (options.presentation_ready != nullptr)
    return options.presentation_ready(state.renderer, state.swapchain, state.swapchain_info);
  return GRANIT_SUCCESS;
}

granit_result resize_swapchain_if_needed() {
  granit_window_state window_state = GRANIT_WINDOW_STATE_INIT;
  auto result = granit_window_get_state(web_host.window_system_handle(), web_host.window_handle(),
                                        &window_state);
  if (result != GRANIT_SUCCESS)
    return result;
  result = granit_swapchain_get_info(state.renderer, state.swapchain, &state.swapchain_info);
  if (result != GRANIT_SUCCESS ||
      (state.swapchain_info.width == window_state.framebuffer_width &&
       state.swapchain_info.height == window_state.framebuffer_height)) {
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
    result = granit_swapchain_get_info(state.renderer, state.swapchain, &state.swapchain_info);
  if (result == GRANIT_SUCCESS)
    ++state.resize_count;
  return result;
}

granit::result execute_web_frame(granit::example::model_viewer::frame_packet&& packet,
                                 granit::example::model_viewer::frame_execution_result& output) {
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
  if (result == GRANIT_SUCCESS &&
      packet.viewer.draw_bindings.size() > std::numeric_limits<std::uint32_t>::max()) {
    result = GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (result == GRANIT_SUCCESS) {
    try {
      std::vector<granit_render_pipeline_draw_binding> bindings;
      bindings.reserve(packet.viewer.draw_bindings.size());
      for (const auto& binding : packet.viewer.draw_bindings) {
        bindings.push_back({.payload = binding.payload,
                            .mesh = binding.mesh.native_handle(),
                            .material = binding.material.native_handle(),
                            .reserved = 0});
      }
      const granit_render_pipeline_environment environment{
          .struct_size = GRANIT_RENDER_PIPELINE_ENVIRONMENT_VERSION_1_SIZE,
          .reserved = 0,
          .irradiance = packet.viewer.environment.irradiance.native_handle(),
          .prefiltered_environment =
              packet.viewer.environment.prefiltered_environment.native_handle(),
          .brdf_lut = packet.viewer.environment.brdf_lut.native_handle(),
          .rotation_radians = packet.viewer.environment.rotation_radians,
          .intensity = packet.viewer.environment.intensity,
          .prefiltered_max_mip = packet.viewer.environment.prefiltered_max_mip,
          .reserved_tail = 0,
      };
      const granit_render_pipeline_render_desc render{
          .struct_size = GRANIT_RENDER_PIPELINE_RENDER_DESC_VERSION_1_SIZE,
          .reserved = 0,
          .scene = packet.viewer.snapshot.native_handle(),
          .output = backbuffer_view,
          .output_format = state.swapchain_info.format,
          .width = packet.viewer.width,
          .height = packet.viewer.height,
          .first_view = 0,
          .view_count = 1,
          .exposure_ev = packet.viewer.exposure_ev,
          .draw_binding_count = static_cast<std::uint32_t>(bindings.size()),
          .draw_bindings = bindings.data(),
          .output_count = 0,
          .outputs = nullptr,
          .frame = frame,
          .reserved_tail = 0,
          .canvas = GRANIT_NULL_HANDLE,
          .debug_draw = GRANIT_NULL_HANDLE,
          .clear_color = {packet.viewer.clear_color.red, packet.viewer.clear_color.green,
                          packet.viewer.clear_color.blue, packet.viewer.clear_color.alpha},
          .environment = &environment,
      };
      result = granit_render_pipeline_render(state.renderer, state.pipeline, &render);
    } catch (const std::bad_alloc&) {
      result = GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      result = GRANIT_ERROR_INTERNAL;
    }
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
  result = granit_swapchain_get_info(state.renderer, state.swapchain, &state.swapchain_info);
  granit::example::model_viewer::frame_packet output;
  granit::example::model_viewer::application_tick_input input;
  input.input = state.input.finish(false, false);
  if (input.input.pointer_delta_x != 0.0F || input.input.pointer_delta_y != 0.0F ||
      input.input.wheel_delta != 0.0F || input.input.focus_requested || input.input.home_requested)
    ++state.applied_input_count;
  state.input.begin_frame();
  input.width = state.swapchain_info.width;
  input.height = state.swapchain_info.height;
  if (result == GRANIT_SUCCESS)
    result = granit::to_native(state.core.tick(input, output.viewer));
  if (result == GRANIT_SUCCESS) {
    granit::example::model_viewer::frame_execution_result execution;
    result = granit::to_native(state.executor.submit(std::move(output), execution));
  }
  if (result == GRANIT_SUCCESS)
    ++state.rendered_frame_count;
  return result;
}

granit_result execute_render_quality_change(granit_sample_count sample_count, unsigned enable_fxaa,
                                            unsigned enable_specular_aa,
                                            unsigned sampler_anisotropy) {
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
    result = granit::to_native(state.core.reupload_scene(
        granit::renderer_ref::from_native(state.renderer), static_cast<float>(sampler_anisotropy)));
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

granit_result configure_render_quality(granit_sample_count sample_count, unsigned enable_fxaa,
                                       unsigned enable_specular_aa,
                                       unsigned sampler_anisotropy) noexcept {
  return granit::to_native(state.executor.run_task([=] {
    return granit::from_native(execute_render_quality_change(
        sample_count, enable_fxaa, enable_specular_aa, sampler_anisotropy));
  }));
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

void update_web_application() noexcept {
  if (state.status == startup_status::failed) {
    return;
  }
  try {
    state.runtime.poll_loading();
  } catch (const std::bad_alloc&) {
    fail("asset-allocation", GRANIT_ERROR_OUT_OF_MEMORY);
    return;
  } catch (...) {
    fail("asset-exception", GRANIT_ERROR_INTERNAL);
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
    const auto result = state.runtime.renderer_ready();
    if (result != granit::result::success) {
      fail("core-renderer-ready", granit::to_native(result));
      return;
    }
    state.core_renderer_ready = true;
  }
  if (state.runtime.loading_status() ==
      granit::example::model_viewer::model_loading_status::failed) {
    fail("asset-fetch", granit::to_native(state.runtime.loading_result()));
    return;
  }
  if (state.runtime.loading_status() !=
      granit::example::model_viewer::model_loading_status::assets_ready) {
    return;
  }

  try {
    if (!state.asset_ready) {
      state.upload_active = true;
      state.upload_cancel_requested = false;
      auto result = state.runtime.prepare_scene(report_load_progress, nullptr);
      if (result != granit::result::success) {
        state.upload_active = false;
        fail("asset-load", granit::to_native(result));
        return;
      }
      result = state.executor.run_task([] {
        return state.core.upload(granit::renderer_ref::from_native(state.renderer), {}, 8.0F,
                                 report_upload_progress, nullptr);
      });
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

  if (!state.pipeline_validation.started()) {
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
    if (!state.pipeline_validation.started()) {
      state.upload_active = true;
      state.upload_cancel_requested = false;
      const auto begin_result = state.pipeline_validation.begin(
          state.renderer, state.core.scene_gpu(), swapchain_info.format, state.sample_count);
      if (begin_result != GRANIT_SUCCESS) {
        state.upload_active = false;
        fail("renderer-pipeline", begin_result);
        return;
      }
    }
    const auto pipeline_result = state.pipeline_validation.poll();
    if (pipeline_result == GRANIT_ERROR_NOT_READY)
      return;
    state.upload_active = false;
    if (pipeline_result != GRANIT_SUCCESS) {
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

granit_result destroy_web_render_resources() noexcept {
  state.runtime.reset();

  auto first_error = GRANIT_SUCCESS;
  const auto capture = [&](granit_result result) {
    if (first_error == GRANIT_SUCCESS && result != GRANIT_SUCCESS)
      first_error = result;
  };
  state.pipeline_validation.reset();
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
  state.shutdown_result = first_error;
  state.shutdown_complete = true;
  state.status = startup_status::stopped;
  return first_error;
}

granit_result shutdown_web_resources() noexcept {
  state.runtime.cancel_loading();
  return granit::to_native(state.executor.run_task(
      [] { return granit::from_native(destroy_web_render_resources()); }));
}

granit::result web_application_host::on_host_initialize() noexcept {
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.presentation_mode = GRANIT_PRESENTATION_ENABLED;
  desc.diagnostic_callback = diagnose;
  auto result = granit_renderer_create(&desc, &state.renderer);
  if (result != GRANIT_SUCCESS) {
    fail("provider-open", result);
    return granit::from_native(result);
  }
  const auto executor_result = state.executor.initialize(execute_web_frame);
  if (executor_result.failed()) {
    fail("executor-initialize", granit::to_native(executor_result));
    return executor_result;
  }
  const auto core_result = state.runtime.begin_renderer();
  if (core_result != granit::result::success) {
    fail("core-renderer-begin", granit::to_native(core_result));
    return core_result;
  }
  try {
    state.asset_url = selected_model_url();
    if (!assets().mount_location(state.asset_url, state.asset_mount, state.asset_path) ||
        !state.runtime.start_loading(assets(), {state.asset_mount, state.asset_path})) {
      fail("asset-fetch-start");
      return granit::result::initialization_failed;
    }
  } catch (const std::bad_alloc&) {
    fail("asset-allocation", GRANIT_ERROR_OUT_OF_MEMORY);
    return granit::result::out_of_memory;
  } catch (...) {
    fail("asset-exception", GRANIT_ERROR_INTERNAL);
    return granit::result::internal;
  }
  state.renderer_initialization_started_ms = emscripten_get_now();
  state.status = startup_status::provider_pending;
  return granit::result::success;
}

granit::result web_application_host::on_host_update(float,
                                                    granit::window_loop_action& action) noexcept {
  update_web_application();
  if (state.status == startup_status::failed)
    action = granit::window_loop_action::idle;
  return granit::result::success;
}

granit::result
web_application_host::on_host_window_event(const granit::window_event& event) noexcept {
  if (event.type == granit::window_event_type::focus_changed)
    ++state.input_event_count;
  state.input.process(event);
  return granit::result::success;
}

granit::result
web_application_host::on_host_input_event(const granit::input_event& event) noexcept {
  ++state.input_event_count;
  state.input.process(event);
  return granit::result::success;
}

void web_application_host::on_host_shutdown(granit::result) noexcept {
  static_cast<void>(shutdown_web_resources());
}

} // namespace

int granit::example::model_viewer::web::runtime_control::platform_status() noexcept {
  return static_cast<int>(state.status);
}

unsigned granit::example::model_viewer::web::runtime_control::input_event_count() noexcept {
  return state.input_event_count;
}

unsigned granit::example::model_viewer::web::runtime_control::rendered_frame_count() noexcept {
  return state.rendered_frame_count;
}

unsigned granit::example::model_viewer::web::runtime_control::applied_input_count() noexcept {
  return state.applied_input_count;
}

unsigned granit::example::model_viewer::web::runtime_control::resize_count() noexcept {
  return state.resize_count;
}

int granit::example::model_viewer::web::runtime_control::configure_render_quality(
    unsigned sample_count, unsigned enable_fxaa, unsigned enable_specular_aa,
    unsigned sampler_anisotropy) {
  return ::configure_render_quality(static_cast<granit_sample_count>(sample_count), enable_fxaa,
                                    enable_specular_aa, sampler_anisotropy);
}

unsigned granit::example::model_viewer::web::runtime_control::quality_generation() noexcept {
  return state.quality_generation;
}

int granit::example::model_viewer::web::runtime_control::configure_lighting(
    float exposure_ev, float environment_intensity, float key_light_intensity) {
  return ::configure_lighting(exposure_ev, environment_intensity, key_light_intensity);
}

unsigned granit::example::model_viewer::web::runtime_control::lighting_generation() noexcept {
  return state.lighting_generation;
}

float granit::example::model_viewer::web::runtime_control::exposure_ev() noexcept {
  return state.core.state().exposure_ev();
}

float granit::example::model_viewer::web::runtime_control::environment_intensity() noexcept {
  return state.core.state().environment_intensity();
}

float granit::example::model_viewer::web::runtime_control::key_light_intensity() noexcept {
  return state.core.state().directional_light().radiance.x;
}

unsigned granit::example::model_viewer::web::runtime_control::max_sampler_anisotropy() noexcept {
  if (state.renderer == GRANIT_NULL_HANDLE)
    return 0;
  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  if (granit_renderer_get_limits(state.renderer, &limits) != GRANIT_SUCCESS)
    return 0;
  return static_cast<unsigned>(limits.max_sampler_anisotropy);
}

unsigned long long
granit::example::model_viewer::web::runtime_control::shutdown_live_resource_count() noexcept {
  return state.shutdown_live_resource_count;
}

unsigned long long
granit::example::model_viewer::web::runtime_control::shutdown_pending_retirement_count() noexcept {
  return state.shutdown_pending_retirement_count;
}

int granit::example::model_viewer::web::runtime_control::shutdown() noexcept {
  if (state.shutdown_complete)
    return state.shutdown_result;
  const auto result = shutdown_web_resources();
  web_host.request_shutdown();
  return result;
}

unsigned granit::example::model_viewer::web::runtime_control::asset_status() noexcept {
  if (state.status == startup_status::failed)
    return 3;
  return state.asset_ready ? 2U : 1U;
}

unsigned granit::example::model_viewer::web::runtime_control::upload_stage() noexcept {
  return static_cast<unsigned>(state.upload_progress.stage);
}

unsigned granit::example::model_viewer::web::runtime_control::upload_completed() noexcept {
  return state.upload_progress.completed;
}

unsigned granit::example::model_viewer::web::runtime_control::upload_total() noexcept {
  return state.upload_progress.total;
}

int granit::example::model_viewer::web::runtime_control::cancel_loading() noexcept {
  if (!state.upload_active)
    return GRANIT_ERROR_NOT_READY;
  state.upload_cancel_requested = true;
  return GRANIT_SUCCESS;
}

unsigned granit::example::model_viewer::web::runtime_control::renderer_state() noexcept {
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  return granit_renderer_get_status(state.renderer, &status) == GRANIT_SUCCESS ? status.state : 0;
}

int granit::example::model_viewer::web::runtime_control::renderer_failure_result() noexcept {
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  return granit_renderer_get_status(state.renderer, &status) == GRANIT_SUCCESS
             ? status.failure_result
             : GRANIT_ERROR_INVALID_HANDLE;
}

int granit::example::model_viewer::web::run_application(const application_options& configuration) {
  runtime_control::ensure_browser_api_linked();
  options = configuration;
  double width = 0.0;
  double height = 0.0;
  if (emscripten_get_element_css_size("#canvas", &width, &height) != EMSCRIPTEN_RESULT_SUCCESS ||
      width < 1.0 || height < 1.0) {
    fail("window-create", GRANIT_ERROR_BACKEND_UNAVAILABLE);
    return 1;
  }
  const auto result = web_host.run_host(
      {.executable_path = "granit_sample_model_viewer_web",
       .title = "Granit Model Viewer",
       .window_system = {.backend = granit::window_backend::emscripten},
       .window_flags = granit::window_flag::visible | granit::window_flag::resizable |
                       granit::window_flag::high_dpi,
       .width = static_cast<std::uint32_t>(std::lround(width)),
       .height = static_cast<std::uint32_t>(std::lround(height))});
  return result.failed() ? 1 : 0;
}
