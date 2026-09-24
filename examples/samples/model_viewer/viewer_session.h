// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_SESSION_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_SESSION_H_

#include "model_viewer/application_core.h"
#include "model_viewer/model_loading_session.h"

#include <utility>

namespace granit::example::model_viewer {

/** 拥有资产加载与 Viewer Core 状态；不拥有平台 I/O、Window 或 GPU 执行策略。 */
class viewer_session final {
public:
  [[nodiscard]] granit::result begin_renderer() noexcept;
  [[nodiscard]] granit::result renderer_ready() noexcept;
  [[nodiscard]] bool start_loading(assets::asset_system& assets, assets::asset_key model);
  void poll_loading();
  /** 完成 CPU 导入并把 Scene 与 GPU 计划交给 Core；调用期间不能并发访问 Core。 */
  [[nodiscard]] granit::result prepare_scene(gltf::import_progress_callback progress = nullptr,
                                             void* progress_user_data = nullptr);
  void cancel_loading() noexcept;
  void reset() noexcept;

  [[nodiscard]] model_loading_status loading_status() const noexcept;
  [[nodiscard]] model_loading_error loading_error() const noexcept;
  [[nodiscard]] granit::result loading_result() const noexcept;
  [[nodiscard]] const std::string& loading_diagnostic() const noexcept;
  [[nodiscard]] gltf::document_load_progress loading_progress() const noexcept;

  [[nodiscard]] granit::result tick(const application_tick_input& input, viewer_frame& output) {
    return core_.tick(input, output);
  }
  [[nodiscard]] granit::result upload(granit::renderer_ref renderer,
                                      std::span<const std::byte> environment_bytes,
                                      float sampler_anisotropy,
                                      gpu_scene_upload_callback progress = nullptr,
                                      void* progress_user_data = nullptr) {
    return core_.upload(renderer, environment_bytes, sampler_anisotropy, progress,
                        progress_user_data);
  }
  [[nodiscard]] granit::result reupload_scene(granit::renderer_ref renderer,
                                              float sampler_anisotropy) {
    return core_.reupload_scene(renderer, sampler_anisotropy);
  }
  [[nodiscard]] granit::result update_material(std::uint32_t material_index,
                                               const material_factor_edit& edit) noexcept {
    return core_.scene_gpu().update_material_factors(core_.cpu_scene(), material_index, edit);
  }
  void fail(granit::result result, std::string diagnostic) {
    core_.fail(result, std::move(diagnostic));
  }

  [[nodiscard]] application_phase phase() const noexcept { return core_.phase(); }
  [[nodiscard]] const std::string& diagnostic() const noexcept { return core_.diagnostic(); }
  [[nodiscard]] gltf::scene& cpu_scene() noexcept { return core_.cpu_scene(); }
  [[nodiscard]] const gltf::scene& cpu_scene() const noexcept { return core_.cpu_scene(); }
  [[nodiscard]] gpu_scene& scene_gpu() noexcept { return core_.scene_gpu(); }
  [[nodiscard]] viewer_state& state() noexcept { return core_.state(); }
  [[nodiscard]] performance_history& performance() noexcept { return core_.performance(); }

private:
  void fail_from_loading();

  application_core core_;
  model_loading_session loading_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_SESSION_H_
