// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_APP_MODEL_VIEWER_APP_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_APP_MODEL_VIEWER_APP_H_

#include "application/application_host.h"
#include "gltf/importer.h"
#include "gltf_rendering/scene_resources.h"
#include "model_viewer/ui/viewer_panels.h"

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/swapchain.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace granit::example::model_viewer {

class render_dispatcher;

enum class viewer_execution_mode { inline_current_thread, dedicated_thread };

/** 应用状态值保持稳定，浏览器验收接口直接映射这些值。 */
enum class model_viewer_app_status : int { failed = -1, starting, loading, ready, stopped };

class model_viewer_app_observer {
public:
  virtual ~model_viewer_app_observer() = default;

  virtual void on_status(model_viewer_app_status, std::string_view, granit::result) noexcept {}
  virtual void on_diagnostic(granit::diagnostic_severity, std::string_view) noexcept {}
  [[nodiscard]] virtual bool on_scene_prepare_progress(const gltf::import_progress&) noexcept {
    return true;
  }
  [[nodiscard]] virtual bool
  on_gpu_upload_progress(const gltf_rendering::scene_upload_progress&) noexcept {
    return true;
  }
  [[nodiscard]] virtual granit::result on_renderer_ready(render_dispatcher&) noexcept {
    return granit::result::success;
  }
  [[nodiscard]] virtual granit::result on_presentation_ready(render_dispatcher&) noexcept {
    return granit::result::success;
  }
  /** 可以返回 not_ready，让状态机在后续 tick 继续等待测试或平台准备。 */
  [[nodiscard]] virtual granit::result on_pipeline_ready(render_dispatcher&) noexcept {
    return granit::result::success;
  }
  virtual void on_shutdown() noexcept {}
};

struct model_viewer_app_desc {
  application_host_desc host;
  std::string model_location;
  std::string environment_location;
  std::string profile_output_path;
  granit::renderer_backend renderer_backend{granit::renderer_backend::automatic};
  granit::present_mode present_mode{granit::present_mode::fifo};
  viewer_execution_mode execution{viewer_execution_mode::inline_current_thread};
  render_quality_config initial_quality{.sample_count = granit::sample_count::one,
                                        .enable_fxaa = true,
                                        .enable_specular_aa = true,
                                        .sampler_anisotropy = 1.0F};
  model_viewer_app_observer* observer{};
  bool enable_validation{};
  bool show_ui{true};
  bool smoke_test{};
  bool keep_alive_on_failure{};
};

/** Desktop/Web 共用的 Model Viewer 生命周期与帧状态机。 */
class model_viewer_app final : public application_host {
public:
  model_viewer_app();
  ~model_viewer_app() override;
  model_viewer_app(const model_viewer_app&) = delete;
  model_viewer_app& operator=(const model_viewer_app&) = delete;

  [[nodiscard]] granit::result run(const model_viewer_app_desc& desc) noexcept;
  [[nodiscard]] granit::result shutdown_resources() noexcept;
  void request_shutdown() noexcept { request_stop(); }

  [[nodiscard]] model_viewer_app_status status() const noexcept;
  [[nodiscard]] unsigned input_event_count() const noexcept;
  [[nodiscard]] unsigned rendered_frame_count() const noexcept;
  [[nodiscard]] unsigned applied_input_count() const noexcept;
  [[nodiscard]] unsigned resize_count() const noexcept;
  [[nodiscard]] unsigned quality_generation() const noexcept;
  [[nodiscard]] unsigned lighting_generation() const noexcept;
  [[nodiscard]] unsigned asset_status() const noexcept;
  [[nodiscard]] gltf_rendering::scene_upload_progress upload_progress() const noexcept;
  [[nodiscard]] std::uint64_t shutdown_live_resource_count() const noexcept;
  [[nodiscard]] std::uint64_t shutdown_pending_retirement_count() const noexcept;
  [[nodiscard]] granit::result shutdown_result() const noexcept;
  [[nodiscard]] granit::result configure_render_quality(const render_quality_config& quality);
  [[nodiscard]] granit::result configure_lighting(float exposure_ev, float environment_intensity,
                                                  float key_light_intensity);
  [[nodiscard]] granit::result cancel_loading() noexcept;
  [[nodiscard]] granit::result
  query_renderer_status(granit::renderer_status& output) const noexcept;
  [[nodiscard]] float exposure_ev() const noexcept;
  [[nodiscard]] float environment_intensity() const noexcept;
  [[nodiscard]] float key_light_intensity() const noexcept;
  [[nodiscard]] float max_sampler_anisotropy() const noexcept;

private:
  struct implementation;

  [[nodiscard]] granit::result on_host_initialize() noexcept override;
  [[nodiscard]] granit::result on_host_update(float delta_seconds,
                                              granit::window_loop_action& action) noexcept override;
  [[nodiscard]] granit::result
  on_host_window_event(const granit::window_event& event) noexcept override;
  [[nodiscard]] granit::result
  on_host_input_event(const granit::input_event& event) noexcept override;
  void on_host_shutdown(granit::result reason) noexcept override;

  std::unique_ptr<implementation> state_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_APP_MODEL_VIEWER_APP_H_
