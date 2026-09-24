// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_BROWSER_TEST_CONTROL_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_BROWSER_TEST_CONTROL_H_

namespace granit::example::model_viewer::web::browser_test_control {

/** 保证静态库链接时保留浏览器导出编译单元。 */
void ensure_browser_api_linked() noexcept;
[[nodiscard]] int platform_status() noexcept;
[[nodiscard]] unsigned input_event_count() noexcept;
[[nodiscard]] unsigned rendered_frame_count() noexcept;
[[nodiscard]] unsigned applied_input_count() noexcept;
[[nodiscard]] unsigned resize_count() noexcept;
[[nodiscard]] int configure_render_quality(unsigned sample_count, unsigned enable_fxaa,
                                           unsigned enable_specular_aa,
                                           unsigned sampler_anisotropy);
[[nodiscard]] unsigned quality_generation() noexcept;
[[nodiscard]] int configure_lighting(float exposure_ev, float environment_intensity,
                                     float key_light_intensity);
[[nodiscard]] unsigned lighting_generation() noexcept;
[[nodiscard]] float exposure_ev() noexcept;
[[nodiscard]] float environment_intensity() noexcept;
[[nodiscard]] float key_light_intensity() noexcept;
[[nodiscard]] unsigned max_sampler_anisotropy() noexcept;
[[nodiscard]] unsigned long long shutdown_live_resource_count() noexcept;
[[nodiscard]] unsigned long long shutdown_pending_retirement_count() noexcept;
[[nodiscard]] int shutdown() noexcept;
[[nodiscard]] unsigned asset_status() noexcept;
[[nodiscard]] unsigned upload_stage() noexcept;
[[nodiscard]] unsigned upload_completed() noexcept;
[[nodiscard]] unsigned upload_total() noexcept;
[[nodiscard]] int cancel_loading() noexcept;
[[nodiscard]] unsigned renderer_state() noexcept;
[[nodiscard]] int renderer_failure_result() noexcept;

} // namespace granit::example::model_viewer::web::browser_test_control

#endif
