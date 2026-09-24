// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "application/application_host.h"
#include "model_viewer/application_core.h"
#include "model_viewer/model_viewer_runtime.h"
#include "model_viewer/render_service.h"
#include "model_viewer/render_task_executor.h"
#include "model_viewer/viewer_input_accumulator.h"
#include "model_viewer/viewer_panels.h"
#include "model_viewer/viewer_ui.h"

#include "application.h"
#include "pipeline_warmup.h"
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
#include "browser_test_control.h"
#include "pipeline_validation.h"
#endif

namespace {

granit::example::model_viewer::web::application_options options;

enum class startup_status : int { failed = -1, starting, provider_pending, ready, stopped };

struct web_platform_state {
  granit::example::model_viewer::inline_render_task_executor executor;
  granit::example::model_viewer::render_service rendering;
  granit::example::model_viewer::web::pipeline_warmup pipeline_warmup;
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
  granit::example::model_viewer::web::pipeline_validation pipeline_validation;
#endif
  granit::example::model_viewer::viewer_ui ui;
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
  double pipeline_warmup_started_ms{};
  granit::example::model_viewer::gpu_scene_upload_progress upload_progress{};
  granit::example::model_viewer::performance_sample latest_performance{};
  std::vector<granit::example::model_viewer::texture_preview> previews;
};

web_platform_state state;

class web_application_host final : public granit::example::application_host {
public:
  [[nodiscard]] granit::window& window() noexcept { return app_window(); }
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
  granit::window_state window_state;
  auto result = web_host.window().get_state(window_state);
  if (result.failed())
    return granit::to_native(result);
  result = state.rendering.initialize_presentation(web_host.window(),
                                                   {.width = window_state.framebuffer_width,
                                                    .height = window_state.framebuffer_height,
                                                    .minimum_image_count = 2},
                                                   true);
  const auto& info = state.rendering.swapchain_info();
  if (result.failed() || info.width == 0 || info.height == 0 || info.image_count == 0) {
    return result.ok() ? GRANIT_ERROR_INITIALIZATION_FAILED : granit::to_native(result);
  }
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
  if (options.presentation_ready != nullptr) {
    const granit_swapchain_info native_info{
        .struct_size = sizeof(granit_swapchain_info),
        .width = info.width,
        .height = info.height,
        .image_count = info.image_count,
        .present_mode = static_cast<granit_present_mode>(info.presentation),
        .format = static_cast<granit_texture_format>(info.format),
    };
    return options.presentation_ready(state.rendering.native_renderer(),
                                      state.rendering.native_swapchain(), native_info);
  }
#endif
  return GRANIT_SUCCESS;
}

granit_result resize_swapchain_if_needed() {
  granit::window_state window_state;
  auto result = web_host.window().get_state(window_state);
  if (result.failed())
    return granit::to_native(result);
  const auto& info = state.rendering.swapchain_info();
  if (info.width == window_state.framebuffer_width &&
      info.height == window_state.framebuffer_height)
    return GRANIT_SUCCESS;
  result = state.rendering.recreate_swapchain({.width = window_state.framebuffer_width,
                                               .height = window_state.framebuffer_height,
                                               .minimum_image_count = 2});
  if (result.ok())
    ++state.resize_count;
  return granit::to_native(result);
}

granit_result execute_render_quality_change(granit_sample_count sample_count, unsigned enable_fxaa,
                                            unsigned enable_specular_aa,
                                            unsigned sampler_anisotropy);

const char* present_mode_label(granit::present_mode mode) noexcept {
  switch (mode) {
  case granit::present_mode::mailbox:
    return "Mailbox";
  case granit::present_mode::immediate:
    return "Immediate";
  default:
    return "FIFO";
  }
}

granit::result rebuild_previews() {
  for (const auto& preview : state.previews)
    static_cast<void>(state.ui.unregister_texture(preview.texture));
  state.previews.clear();
  const auto register_preview = [](const granit::example::gltf::texture_reference& reference,
                                   bool srgb) {
    using namespace granit::example::model_viewer;
    if (reference.image == granit::example::gltf::invalid_index)
      return granit::result::success;
    ImTextureID existing = ImTextureID_Invalid;
    if (find_texture_preview(reference, srgb, state.previews, existing))
      return granit::result::success;
    granit::texture_view_ref view;
    granit::sampler_ref sampler;
    auto result = state.core.scene_gpu().texture_binding(reference, srgb, view, sampler);
    ImTextureID texture = ImTextureID_Invalid;
    if (result.ok())
      result = state.ui.register_texture(view, sampler, texture);
    if (result.ok())
      state.previews.push_back({reference.image, reference.sampler, srgb, texture});
    return result;
  };
  for (const auto& material : state.core.cpu_scene().materials) {
    granit::result result;
    if ((result = register_preview(material.base_color_texture, true)).failed() ||
        (result = register_preview(material.emissive_texture, true)).failed() ||
        (result = register_preview(material.metallic_roughness_texture, false)).failed() ||
        (result = register_preview(material.normal_texture, false)).failed() ||
        (result = register_preview(material.occlusion_texture, false)).failed())
      return result;
  }
  return granit::result::success;
}

granit_result render_model_viewer_frame(float delta_seconds) {
  auto result = resize_swapchain_if_needed();
  if (result != GRANIT_SUCCESS)
    return result;
  granit::window_state window_state;
  auto operation = web_host.window().get_state(window_state);
  if (operation.failed())
    return granit::to_native(operation);
  const auto& renderer_info = state.rendering.renderer_info();
  const auto& renderer_limits = state.rendering.renderer_limits();
  const auto& swapchain_info = state.rendering.swapchain_info();
  state.ui.begin_frame(window_state, delta_seconds);
  const granit::example::model_viewer::renderer_panel_info panel_renderer{
      .backend = "WebGPU",
      .adapter = renderer_info.adapter_name,
      .swapchain_format = "Swapchain",
      .present_mode = present_mode_label(swapchain_info.presentation),
      .width = swapchain_info.width,
      .height = swapchain_info.height,
      .frame_slots = GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
      .supported_sample_counts = renderer_limits.framebuffer_sample_counts,
      .max_sampler_anisotropy = renderer_limits.max_sampler_anisotropy};
  const granit::example::model_viewer::performance_panel_info panel_performance{
      .frames_per_second = state.latest_performance.frames_per_second,
      .cpu_frame_ms = state.latest_performance.cpu_frame_ms,
      .frame_slot_wait_ms = state.latest_performance.frame_slot_wait_ms,
      .present_wait_ms = state.latest_performance.present_wait_ms,
      .gpu_frame_ms = state.latest_performance.gpu_frame_ms,
      .gpu_timing_available = state.latest_performance.gpu_timing_available,
      .history = state.core.performance().summarize()};
  const granit::example::model_viewer::render_quality_config quality{
      .sample_count = state.sample_count,
      .enable_fxaa = state.enable_fxaa != 0,
      .enable_specular_aa = state.enable_specular_aa != 0,
      .sampler_anisotropy = static_cast<float>(state.sampler_anisotropy)};
  const auto changes = granit::example::model_viewer::draw_viewer_panels(
      state.core.cpu_scene(), state.core.state(), panel_renderer, panel_performance, quality,
      state.previews);
  if (changes.quality) {
    result = execute_render_quality_change(
        changes.quality->sample_count, changes.quality->enable_fxaa ? 1U : 0U,
        changes.quality->enable_specular_aa ? 1U : 0U,
        static_cast<unsigned>(changes.quality->sampler_anisotropy));
    if (result != GRANIT_SUCCESS)
      return result;
    if (changes.quality->sampler_anisotropy != quality.sampler_anisotropy) {
      operation = rebuild_previews();
      if (operation.failed())
        return granit::to_native(operation);
    }
  }
  granit::example::model_viewer::frame_packet output;
  granit::example::model_viewer::application_tick_input input;
  input.input = state.input.finish(state.ui.wants_mouse(), state.ui.wants_keyboard());
  if (input.input.pointer_delta_x != 0.0F || input.input.pointer_delta_y != 0.0F ||
      input.input.wheel_delta != 0.0F || input.input.focus_requested || input.input.home_requested)
    ++state.applied_input_count;
  state.input.begin_frame();
  input.change = changes.state;
  input.width = swapchain_info.width;
  input.height = swapchain_info.height;
  input.performance = state.latest_performance;
  if (result == GRANIT_SUCCESS)
    result = granit::to_native(state.core.tick(input, output.viewer));
  if (result == GRANIT_SUCCESS)
    result = granit::to_native(state.ui.capture(output.canvas));
  if (result == GRANIT_SUCCESS && changes.material &&
      state.core.state().selected_material() != granit::example::gltf::invalid_index) {
    result = granit::to_native(
        state.rendering.update_material(state.core.state().selected_material(), *changes.material));
  }
  if (result == GRANIT_SUCCESS) {
    granit::example::model_viewer::frame_execution_result execution;
    result = granit::to_native(state.rendering.submit(std::move(output), execution));
    if (result == GRANIT_SUCCESS) {
      state.latest_performance = {.frames_per_second =
                                      delta_seconds > 0.0F ? 1.0F / delta_seconds : 0.0F,
                                  .cpu_frame_ms = delta_seconds * 1000.0F,
                                  .frame_slot_wait_ms = execution.acquire_wait_ms,
                                  .present_wait_ms = execution.present_wait_ms,
                                  .gpu_frame_ms = execution.gpu_frame_ms,
                                  .gpu_timing_available = execution.gpu_timing_available};
    }
  }
  if (result == GRANIT_SUCCESS)
    ++state.rendered_frame_count;
  return result;
}

granit_result execute_render_quality_change(granit_sample_count sample_count, unsigned enable_fxaa,
                                            unsigned enable_specular_aa,
                                            unsigned sampler_anisotropy) {
  if (state.status != startup_status::ready || !state.rendering.valid())
    return GRANIT_ERROR_NOT_READY;
  if ((sample_count != GRANIT_SAMPLE_COUNT_1 && sample_count != GRANIT_SAMPLE_COUNT_4) ||
      enable_fxaa > 1 || enable_specular_aa > 1 || sampler_anisotropy == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  const auto& limits = state.rendering.renderer_limits();
  if ((limits.framebuffer_sample_counts & sample_count) == 0 ||
      static_cast<float>(sampler_anisotropy) > limits.max_sampler_anisotropy)
    return GRANIT_ERROR_UNSUPPORTED;

  const granit::render_pipeline_desc desc{
      .samples = static_cast<granit::sample_count>(sample_count),
      .enable_fxaa = enable_fxaa != 0,
      .enable_specular_aa = enable_specular_aa != 0,
  };
  granit::example::model_viewer::render_quality_change_result output;
  const auto result = granit::to_native(
      state.rendering.change_quality(desc, static_cast<float>(sampler_anisotropy),
                                     sampler_anisotropy != state.sampler_anisotropy, output));
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  state.sample_count = sample_count;
  state.enable_fxaa = enable_fxaa;
  state.enable_specular_aa = enable_specular_aa;
  state.sampler_anisotropy = sampler_anisotropy;
  ++state.quality_generation;
  return GRANIT_SUCCESS;
}

#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
granit_result configure_render_quality(granit_sample_count sample_count, unsigned enable_fxaa,
                                       unsigned enable_specular_aa,
                                       unsigned sampler_anisotropy) noexcept {
  return execute_render_quality_change(sample_count, enable_fxaa, enable_specular_aa,
                                       sampler_anisotropy);
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
#endif

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
    const auto result = render_model_viewer_frame(1.0F / 60.0F);
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
  const auto process_result = state.rendering.process_renderer_events();
  if (process_result.failed()) {
    fail("provider-events", granit::to_native(process_result));
    return;
  }

  granit::renderer_status renderer_status;
  const auto status_result = state.rendering.query_renderer_status(renderer_status);
  if (status_result.failed()) {
    fail("renderer-status", granit::to_native(status_result));
    return;
  }
  if (renderer_status.state == granit::renderer_state::failed ||
      renderer_status.state == granit::renderer_state::device_lost) {
    fail("provider-terminal", granit::to_native(renderer_status.failure_result));
    return;
  }
  if (renderer_status.state != granit::renderer_state::ready) {
    return;
  }
  if (state.runtime.loading_status() ==
      granit::example::model_viewer::model_loading_status::failed) {
    const auto stage = state.runtime.loading_error() ==
                               granit::example::model_viewer::model_loading_error::resource_read
                           ? "asset-resource-fetch"
                           : "asset-fetch";
    fail(stage, granit::to_native(state.runtime.loading_result()));
    return;
  }
  if (!state.core_renderer_ready) {
    auto result = state.rendering.complete_renderer_initialization();
    if (result.ok())
      result = state.runtime.renderer_ready();
    if (result != granit::result::success) {
      fail("core-renderer-ready", granit::to_native(result));
      return;
    }
    state.core_renderer_ready = true;
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
      result = state.rendering.upload_scene({}, 8.0F, report_upload_progress, nullptr);
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

  if (!state.pipeline_warmup.started()) {
    const auto& limits = state.rendering.renderer_limits();
    if (limits.uniform_buffer_offset_alignment == 0 ||
        limits.max_uniform_buffer_binding_size == 0) {
      fail("renderer-limits", GRANIT_ERROR_INTERNAL);
      return;
    }
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
    if (options.renderer_ready != nullptr) {
      granit_renderer_limits native_limits = GRANIT_RENDERER_LIMITS_INIT;
      native_limits.uniform_buffer_offset_alignment = limits.uniform_buffer_offset_alignment;
      native_limits.max_uniform_buffer_binding_size = limits.max_uniform_buffer_binding_size;
      native_limits.framebuffer_sample_counts = limits.framebuffer_sample_counts;
      native_limits.max_sampler_anisotropy = limits.max_sampler_anisotropy;
      native_limits.supported_features = limits.supported_features;
      const auto validation_result =
          options.renderer_ready(state.rendering.native_renderer(), native_limits);
      if (validation_result != GRANIT_SUCCESS) {
        fail("renderer-validation", validation_result);
        return;
      }
    }
#endif
  }
  try {
    if (state.rendering.native_swapchain() == GRANIT_NULL_HANDLE) {
      const auto create_result = create_presentation_resources();
      if (create_result != GRANIT_SUCCESS) {
        fail("presentation-create", create_result);
        return;
      }
    }
    const auto& swapchain_info = state.rendering.swapchain_info();
    if (swapchain_info.format == granit::texture_format::undefined) {
      fail("presentation-info", GRANIT_ERROR_INITIALIZATION_FAILED);
      return;
    }
    if (!state.pipeline_warmup.started()) {
      state.upload_active = true;
      state.upload_cancel_requested = false;
      const auto begin_result = state.pipeline_warmup.begin(
          state.rendering.native_renderer(), state.core.scene_gpu(),
          static_cast<granit_texture_format>(swapchain_info.format), state.sample_count);
      if (begin_result != GRANIT_SUCCESS) {
        state.upload_active = false;
        fail("renderer-pipeline", begin_result);
        return;
      }
      state.pipeline_warmup_started_ms = emscripten_get_now();
    }
    auto pipeline_result = state.pipeline_warmup.poll();
    while (pipeline_result == GRANIT_ERROR_NOT_READY && !state.upload_cancel_requested &&
           emscripten_get_now() - state.pipeline_warmup_started_ms < 30000.0) {
      // Asyncify 允许浏览器交付 WaitAnyOnly Pipeline Future；某些 Emscripten 主循环不会在
      // 当前回调包含前序 Asyncify 上传后再次调度 tick，因此在同一启动阶段显式让出并轮询。
      emscripten_sleep(0);
      pipeline_result = granit::to_native(state.rendering.process_renderer_events());
      if (pipeline_result == GRANIT_SUCCESS)
        pipeline_result = state.pipeline_warmup.poll();
    }
    if (pipeline_result == GRANIT_ERROR_NOT_READY && state.upload_cancel_requested)
      pipeline_result = GRANIT_ERROR_CANCELLED;
    state.upload_active = false;
    if (pipeline_result != GRANIT_SUCCESS) {
      fail("renderer-pipeline", pipeline_result);
      return;
    }
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
    if (!state.pipeline_validation.started()) {
      const auto validation_begin =
          state.pipeline_validation.begin(state.rendering.native_renderer());
      if (validation_begin != GRANIT_SUCCESS) {
        fail("renderer-pipeline-validation", validation_begin);
        return;
      }
    }
    auto validation_result = state.pipeline_validation.poll();
    while (validation_result == GRANIT_ERROR_NOT_READY && !state.upload_cancel_requested &&
           emscripten_get_now() - state.pipeline_warmup_started_ms < 30000.0) {
      emscripten_sleep(0);
      validation_result = granit::to_native(state.rendering.process_renderer_events());
      if (validation_result == GRANIT_SUCCESS)
        validation_result = state.pipeline_validation.poll();
    }
    if (validation_result != GRANIT_SUCCESS) {
      fail("renderer-pipeline-validation", validation_result);
      return;
    }
#endif
    const auto pipeline_create_result = state.rendering.initialize_pipeline({});
    if (pipeline_create_result.failed()) {
      fail("pipeline-create", granit::to_native(pipeline_create_result));
      return;
    }
    std::vector<std::byte> font_pixels;
    std::uint32_t font_width{};
    std::uint32_t font_height{};
    auto ui_result = state.ui.capture_font_atlas(font_pixels, font_width, font_height);
    if (ui_result.ok())
      ui_result = state.rendering.initialize_font_atlas(font_pixels, font_width, font_height);
    if (ui_result.ok())
      ui_result =
          state.ui.register_font(state.rendering.font_view(), state.rendering.font_sampler());
    if (ui_result.ok())
      ui_result = rebuild_previews();
    if (ui_result.failed()) {
      fail("viewer-ui", granit::to_native(ui_result));
      return;
    }
    const auto render_result = render_model_viewer_frame(1.0F / 60.0F);
    if (render_result != GRANIT_SUCCESS) {
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
  static_cast<void>(state.rendering.finish_loading());
  std::puts("GRANIT_STATUS:ready");
}

granit_result destroy_web_render_resources() noexcept {
  state.ui.clear_textures();
  state.previews.clear();
  state.runtime.reset();
  state.pipeline_warmup.reset();
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
  state.pipeline_validation.reset();
#endif
  granit::renderer_resource_stats stats;
  auto result = state.rendering.shutdown(&stats);
  state.shutdown_live_resource_count = stats.total_live_count;
  state.shutdown_pending_retirement_count = stats.pending_retirement_count;
  if (result.ok() && stats.total_live_count != 0)
    result = granit::result::internal;
  state.shutdown_result = granit::to_native(result);
  state.shutdown_complete = true;
  state.status = startup_status::stopped;
  return state.shutdown_result;
}

granit_result shutdown_web_resources() noexcept {
  if (state.shutdown_complete)
    return state.shutdown_result;
  state.runtime.cancel_loading();
  return destroy_web_render_resources();
}

granit::result web_application_host::on_host_initialize() noexcept {
  auto ui_result = state.ui.initialize();
  if (ui_result.failed()) {
    fail("viewer-ui-initialize", granit::to_native(ui_result));
    return ui_result;
  }
  const auto result = state.rendering.initialize_renderer(
      state.executor, {.presentation = granit::presentation_mode::enabled, .diagnostics = diagnose},
      state.core);
  if (result.failed()) {
    fail("provider-open", granit::to_native(result));
    return result;
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

granit::result web_application_host::on_host_update(float delta_seconds,
                                                    granit::window_loop_action& action) noexcept {
  if (state.status == startup_status::ready) {
    const auto result = render_model_viewer_frame(delta_seconds);
    if (result != GRANIT_SUCCESS)
      fail("model-viewer-frame", result);
  } else {
    update_web_application();
  }
  if (state.status == startup_status::failed)
    action = granit::window_loop_action::idle;
  return granit::result::success;
}

granit::result
web_application_host::on_host_window_event(const granit::window_event& event) noexcept {
  if (event.type == granit::window_event_type::focus_changed)
    ++state.input_event_count;
  state.ui.process(event);
  state.input.process(event);
  return granit::result::success;
}

granit::result
web_application_host::on_host_input_event(const granit::input_event& event) noexcept {
  ++state.input_event_count;
  state.ui.process(event);
  state.input.process(event, state.ui.wants_mouse(), state.ui.wants_keyboard());
  return granit::result::success;
}

void web_application_host::on_host_shutdown(granit::result) noexcept {
  static_cast<void>(shutdown_web_resources());
}

} // namespace

#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
int granit::example::model_viewer::web::browser_test_control::platform_status() noexcept {
  return static_cast<int>(state.status);
}

unsigned granit::example::model_viewer::web::browser_test_control::input_event_count() noexcept {
  return state.input_event_count;
}

unsigned granit::example::model_viewer::web::browser_test_control::rendered_frame_count() noexcept {
  return state.rendered_frame_count;
}

unsigned granit::example::model_viewer::web::browser_test_control::applied_input_count() noexcept {
  return state.applied_input_count;
}

unsigned granit::example::model_viewer::web::browser_test_control::resize_count() noexcept {
  return state.resize_count;
}

int granit::example::model_viewer::web::browser_test_control::configure_render_quality(
    unsigned sample_count, unsigned enable_fxaa, unsigned enable_specular_aa,
    unsigned sampler_anisotropy) {
  return ::configure_render_quality(static_cast<granit_sample_count>(sample_count), enable_fxaa,
                                    enable_specular_aa, sampler_anisotropy);
}

unsigned granit::example::model_viewer::web::browser_test_control::quality_generation() noexcept {
  return state.quality_generation;
}

int granit::example::model_viewer::web::browser_test_control::configure_lighting(
    float exposure_ev, float environment_intensity, float key_light_intensity) {
  return ::configure_lighting(exposure_ev, environment_intensity, key_light_intensity);
}

unsigned granit::example::model_viewer::web::browser_test_control::lighting_generation() noexcept {
  return state.lighting_generation;
}

float granit::example::model_viewer::web::browser_test_control::exposure_ev() noexcept {
  return state.core.state().exposure_ev();
}

float granit::example::model_viewer::web::browser_test_control::environment_intensity() noexcept {
  return state.core.state().environment_intensity();
}

float granit::example::model_viewer::web::browser_test_control::key_light_intensity() noexcept {
  return state.core.state().directional_light().radiance.x;
}

unsigned
granit::example::model_viewer::web::browser_test_control::max_sampler_anisotropy() noexcept {
  if (!state.rendering.valid())
    return 0;
  return static_cast<unsigned>(state.rendering.renderer_limits().max_sampler_anisotropy);
}

unsigned long long
granit::example::model_viewer::web::browser_test_control::shutdown_live_resource_count() noexcept {
  return state.shutdown_live_resource_count;
}

unsigned long long granit::example::model_viewer::web::browser_test_control::
    shutdown_pending_retirement_count() noexcept {
  return state.shutdown_pending_retirement_count;
}

int granit::example::model_viewer::web::browser_test_control::shutdown() noexcept {
  if (state.shutdown_complete)
    return state.shutdown_result;
  const auto result = shutdown_web_resources();
  web_host.request_shutdown();
  return result;
}

unsigned granit::example::model_viewer::web::browser_test_control::asset_status() noexcept {
  if (state.status == startup_status::failed)
    return 3;
  return state.asset_ready ? 2U : 1U;
}

unsigned granit::example::model_viewer::web::browser_test_control::upload_stage() noexcept {
  return static_cast<unsigned>(state.upload_progress.stage);
}

unsigned granit::example::model_viewer::web::browser_test_control::upload_completed() noexcept {
  return state.upload_progress.completed;
}

unsigned granit::example::model_viewer::web::browser_test_control::upload_total() noexcept {
  return state.upload_progress.total;
}

int granit::example::model_viewer::web::browser_test_control::cancel_loading() noexcept {
  if (!state.upload_active)
    return GRANIT_ERROR_NOT_READY;
  state.upload_cancel_requested = true;
  return GRANIT_SUCCESS;
}

unsigned granit::example::model_viewer::web::browser_test_control::renderer_state() noexcept {
  granit::renderer_status status;
  return state.rendering.query_renderer_status(status).ok() ? static_cast<unsigned>(status.state)
                                                            : 0;
}

int granit::example::model_viewer::web::browser_test_control::renderer_failure_result() noexcept {
  granit::renderer_status status;
  return state.rendering.query_renderer_status(status).ok()
             ? granit::to_native(status.failure_result)
             : GRANIT_ERROR_INVALID_HANDLE;
}
#endif

int granit::example::model_viewer::web::run_application(const application_options& configuration) {
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
  browser_test_control::ensure_browser_api_linked();
#endif
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
