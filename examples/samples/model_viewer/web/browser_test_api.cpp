// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "browser_test_control.h"

#include <new>

#include <emscripten/emscripten.h>

#include <granit/core/result.h>

namespace control = granit::example::model_viewer::web::browser_test_control;

void control::ensure_browser_api_linked() noexcept {}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_platform_status() noexcept {
  return control::platform_status();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_input_event_count() noexcept {
  return control::input_event_count();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_rendered_frame_count() noexcept {
  return control::rendered_frame_count();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_applied_input_count() noexcept {
  return control::applied_input_count();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_resize_count() noexcept {
  return control::resize_count();
}

extern "C" EMSCRIPTEN_KEEPALIVE int
granit_web_configure_render_quality(unsigned sample_count, unsigned enable_fxaa,
                                    unsigned enable_specular_aa,
                                    unsigned sampler_anisotropy) noexcept {
  try {
    return control::configure_render_quality(sample_count, enable_fxaa, enable_specular_aa,
                                             sampler_anisotropy);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_quality_generation() noexcept {
  return control::quality_generation();
}

extern "C" EMSCRIPTEN_KEEPALIVE int
granit_web_configure_lighting(float exposure_ev, float environment_intensity,
                              float key_light_intensity) noexcept {
  try {
    return control::configure_lighting(exposure_ev, environment_intensity, key_light_intensity);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_lighting_generation() noexcept {
  return control::lighting_generation();
}

extern "C" EMSCRIPTEN_KEEPALIVE float granit_web_exposure_ev() noexcept {
  return control::exposure_ev();
}

extern "C" EMSCRIPTEN_KEEPALIVE float granit_web_environment_intensity() noexcept {
  return control::environment_intensity();
}

extern "C" EMSCRIPTEN_KEEPALIVE float granit_web_key_light_intensity() noexcept {
  return control::key_light_intensity();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_max_sampler_anisotropy() noexcept {
  return control::max_sampler_anisotropy();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned long long
granit_web_shutdown_live_resource_count() noexcept {
  return control::shutdown_live_resource_count();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned long long
granit_web_shutdown_pending_retirement_count() noexcept {
  return control::shutdown_pending_retirement_count();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_shutdown() noexcept { return control::shutdown(); }

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_asset_status() noexcept {
  return control::asset_status();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_upload_stage() noexcept {
  return control::upload_stage();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_upload_completed() noexcept {
  return control::upload_completed();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_upload_total() noexcept {
  return control::upload_total();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_cancel_loading() noexcept {
  return control::cancel_loading();
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_renderer_state() noexcept {
  return control::renderer_state();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_renderer_failure_result() noexcept {
  return control::renderer_failure_result();
}
