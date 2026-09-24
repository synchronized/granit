// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_VIEWER_RUNTIME_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_VIEWER_RUNTIME_H_

#include "model_viewer/application_core.h"
#include "model_viewer/model_loading_session.h"

namespace granit::example::model_viewer {

/** 连接资产加载与 Viewer Core 的跨平台生命周期；不拥有平台 I/O 或 GPU 执行策略。 */
class model_viewer_runtime final {
public:
  model_viewer_runtime(application_core& core, model_loading_session& loading) noexcept;

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

private:
  void fail_from_loading();

  application_core* core_{};
  model_loading_session* loading_{};
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_VIEWER_RUNTIME_H_
