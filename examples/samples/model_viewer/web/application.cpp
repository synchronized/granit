// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application.h"

#include "model_viewer/render_service.h"
#include "model_viewer/viewer_application.h"
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
#include "browser_test_control.h"
#include "browser_test_hooks.h"
#include "pipeline_validation.h"
#endif

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using granit::example::model_viewer::render_service;
using granit::example::model_viewer::viewer_application_status;

granit::example::model_viewer::web::application_options options;
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
granit::example::model_viewer::web::browser_test::hooks test_hooks;
#endif

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

class web_observer final : public granit::example::model_viewer::viewer_application_observer {
public:
  void on_status(viewer_application_status status, std::string_view stage,
                 granit::result result) noexcept override {
    if (status == viewer_application_status::ready) {
      std::puts("GRANIT_STATUS:ready");
    } else if (status == viewer_application_status::failed) {
      std::fprintf(stderr, "GRANIT_STATUS:failed:%.*s:%d\n", static_cast<int>(stage.size()),
                   stage.data(), granit::to_native(result));
    }
  }

  void on_diagnostic(granit::diagnostic_severity severity,
                     std::string_view message) noexcept override {
    auto* stream = severity == granit::diagnostic_severity::info ? stdout : stderr;
    std::fprintf(stream, "GRANIT_DIAGNOSTIC:%.*s\n", static_cast<int>(message.size()),
                 message.data());
  }

  bool on_scene_prepare_progress(
      const granit::example::gltf::import_progress& progress) noexcept override {
    std::printf("GRANIT_PROGRESS:%s:%u:%u\n", load_stage_name(progress.stage), progress.completed,
                progress.total);
    emscripten_sleep(0);
    return true;
  }

  bool on_gpu_upload_progress(
      const granit::example::model_viewer::gpu_scene_upload_progress& progress) noexcept override {
    std::printf("GRANIT_PROGRESS:%s:%u:%u\n", upload_stage_name(progress.stage), progress.completed,
                progress.total);
    emscripten_sleep(0);
    return true;
  }

  granit::result on_renderer_ready(render_service& rendering) noexcept override {
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
    if (test_hooks.renderer_ready != nullptr) {
      const auto& limits = rendering.renderer_limits();
      granit_renderer_limits native_limits = GRANIT_RENDERER_LIMITS_INIT;
      native_limits.uniform_buffer_offset_alignment = limits.uniform_buffer_offset_alignment;
      native_limits.max_uniform_buffer_binding_size = limits.max_uniform_buffer_binding_size;
      native_limits.framebuffer_sample_counts = limits.framebuffer_sample_counts;
      native_limits.max_sampler_anisotropy = limits.max_sampler_anisotropy;
      native_limits.supported_features = limits.supported_features;
      return granit::from_native(
          test_hooks.renderer_ready(rendering.native_renderer(), native_limits));
    }
#else
    static_cast<void>(rendering);
#endif
    return granit::result::success;
  }

  granit::result on_presentation_ready(render_service& rendering) noexcept override {
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
    if (test_hooks.presentation_ready != nullptr) {
      const auto& info = rendering.swapchain_info();
      const granit_swapchain_info native_info{
          .struct_size = sizeof(granit_swapchain_info),
          .width = info.width,
          .height = info.height,
          .image_count = info.image_count,
          .present_mode = static_cast<granit_present_mode>(info.presentation),
          .format = static_cast<granit_texture_format>(info.format),
      };
      return granit::from_native(test_hooks.presentation_ready(
          rendering.native_renderer(), rendering.native_swapchain(), native_info));
    }
#else
    static_cast<void>(rendering);
#endif
    return granit::result::success;
  }

  granit::result on_pipeline_ready(render_service& rendering) noexcept override {
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
    if (!validation_.started()) {
      const auto result = validation_.begin(rendering.renderer().native_handle());
      if (result != GRANIT_SUCCESS)
        return granit::from_native(result);
    }
    return granit::from_native(validation_.poll());
#else
    static_cast<void>(rendering);
    return granit::result::success;
#endif
  }

  void on_shutdown() noexcept override {
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
    validation_.reset();
#endif
  }

private:
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
  granit::example::model_viewer::web::pipeline_validation validation_;
#endif
};

web_observer observer;
granit::example::model_viewer::viewer_application application;

std::string selected_model_url() {
  const auto* selected = emscripten_run_script_string(
      "new URLSearchParams(globalThis.location.search).get('model') || ''");
  return selected == nullptr || *selected == '\0' ? std::string{options.default_model_url}
                                                  : std::string{selected};
}

} // namespace

#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
void granit::example::model_viewer::web::browser_test::configure(hooks value) noexcept {
  test_hooks = value;
}

int granit::example::model_viewer::web::browser_test_control::platform_status() noexcept {
  return static_cast<int>(application.status());
}
unsigned granit::example::model_viewer::web::browser_test_control::input_event_count() noexcept {
  return application.input_event_count();
}
unsigned granit::example::model_viewer::web::browser_test_control::rendered_frame_count() noexcept {
  return application.rendered_frame_count();
}
unsigned granit::example::model_viewer::web::browser_test_control::applied_input_count() noexcept {
  return application.applied_input_count();
}
unsigned granit::example::model_viewer::web::browser_test_control::resize_count() noexcept {
  return application.resize_count();
}
int granit::example::model_viewer::web::browser_test_control::configure_render_quality(
    unsigned sample_count, unsigned enable_fxaa, unsigned enable_specular_aa,
    unsigned sampler_anisotropy) {
  if (enable_fxaa > 1 || enable_specular_aa > 1)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::to_native(application.configure_render_quality(
      {.sample_count = sample_count,
       .enable_fxaa = enable_fxaa != 0,
       .enable_specular_aa = enable_specular_aa != 0,
       .sampler_anisotropy = static_cast<float>(sampler_anisotropy)}));
}
unsigned granit::example::model_viewer::web::browser_test_control::quality_generation() noexcept {
  return application.quality_generation();
}
int granit::example::model_viewer::web::browser_test_control::configure_lighting(
    float exposure_ev, float environment_intensity, float key_light_intensity) {
  return granit::to_native(
      application.configure_lighting(exposure_ev, environment_intensity, key_light_intensity));
}
unsigned granit::example::model_viewer::web::browser_test_control::lighting_generation() noexcept {
  return application.lighting_generation();
}
float granit::example::model_viewer::web::browser_test_control::exposure_ev() noexcept {
  return application.exposure_ev();
}
float granit::example::model_viewer::web::browser_test_control::environment_intensity() noexcept {
  return application.environment_intensity();
}
float granit::example::model_viewer::web::browser_test_control::key_light_intensity() noexcept {
  return application.key_light_intensity();
}
unsigned
granit::example::model_viewer::web::browser_test_control::max_sampler_anisotropy() noexcept {
  return static_cast<unsigned>(application.max_sampler_anisotropy());
}
unsigned long long
granit::example::model_viewer::web::browser_test_control::shutdown_live_resource_count() noexcept {
  return application.shutdown_live_resource_count();
}
unsigned long long granit::example::model_viewer::web::browser_test_control::
    shutdown_pending_retirement_count() noexcept {
  return application.shutdown_pending_retirement_count();
}
int granit::example::model_viewer::web::browser_test_control::shutdown() noexcept {
  const auto result = application.shutdown_resources();
  application.request_shutdown();
  return granit::to_native(result);
}
unsigned granit::example::model_viewer::web::browser_test_control::asset_status() noexcept {
  return application.asset_status();
}
unsigned granit::example::model_viewer::web::browser_test_control::upload_stage() noexcept {
  return static_cast<unsigned>(application.upload_progress().stage);
}
unsigned granit::example::model_viewer::web::browser_test_control::upload_completed() noexcept {
  return application.upload_progress().completed;
}
unsigned granit::example::model_viewer::web::browser_test_control::upload_total() noexcept {
  return application.upload_progress().total;
}
int granit::example::model_viewer::web::browser_test_control::cancel_loading() noexcept {
  return granit::to_native(application.cancel_loading());
}
unsigned granit::example::model_viewer::web::browser_test_control::renderer_state() noexcept {
  granit::renderer_status status;
  return application.query_renderer_status(status).ok() ? static_cast<unsigned>(status.state) : 0U;
}
int granit::example::model_viewer::web::browser_test_control::renderer_failure_result() noexcept {
  granit::renderer_status status;
  return application.query_renderer_status(status).ok() ? granit::to_native(status.failure_result)
                                                        : GRANIT_ERROR_INVALID_HANDLE;
}
#endif

int granit::example::model_viewer::web::run_application(const application_options& configuration) {
#if defined(GRANIT_MODEL_VIEWER_BROWSER_TESTS)
  browser_test_control::ensure_browser_api_linked();
#endif
  options = configuration;
  double width{};
  double height{};
  if (emscripten_get_element_css_size("#canvas", &width, &height) != EMSCRIPTEN_RESULT_SUCCESS ||
      width < 1.0 || height < 1.0) {
    std::fprintf(stderr, "GRANIT_STATUS:failed:window-create:%d\n",
                 GRANIT_ERROR_BACKEND_UNAVAILABLE);
    return 1;
  }
  const auto model = selected_model_url();
  const auto result = application.run({
      .host = {.executable_path = "granit_sample_model_viewer_web",
               .title = "Granit Model Viewer",
               .window_system = {.backend = granit::window_backend::emscripten},
               .window_flags = granit::window_flag::visible | granit::window_flag::resizable |
                               granit::window_flag::high_dpi,
               .width = static_cast<std::uint32_t>(std::lround(width)),
               .height = static_cast<std::uint32_t>(std::lround(height))},
      .model_location = model,
      .environment_location = {},
      .profile_output_path = {},
      .renderer_backend = granit::renderer_backend::webgpu,
      .present_mode = granit::present_mode::fifo,
      .execution = viewer_execution_mode::inline_current_thread,
      .initial_quality = {.sample_count = GRANIT_SAMPLE_COUNT_1,
                          .enable_fxaa = true,
                          .enable_specular_aa = true,
                          .sampler_anisotropy = 1.0F},
      .observer = &observer,
      .enable_validation = false,
      .show_ui = true,
      .smoke_test = false,
      .keep_alive_on_failure = true,
  });
  return result.failed() ? 1 : 0;
}
