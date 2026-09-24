// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_LOADING_SESSION_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_LOADING_SESSION_H_

#include "assets/asset_system.h"
#include "gltf/document_loader.h"
#include "gltf/importer.h"
#include "model_viewer/gpu_scene.h"

#include <atomic>
#include <string>

namespace granit::example::model_viewer {

enum class model_loading_status {
  idle,
  loading_assets,
  assets_ready,
  preparing,
  ready,
  failed,
  cancelled,
};

/** 统一模型文档、外部资源、CPU Scene 与 GPU 创建计划的加载状态。 */
class model_loading_session final {
public:
  [[nodiscard]] bool start(assets::asset_system& assets, assets::asset_key model);
  void poll();
  [[nodiscard]] granit::result prepare(gltf::import_progress_callback progress = nullptr,
                                       void* progress_user_data = nullptr);
  [[nodiscard]] bool take(gltf::scene& scene, gpu_scene_plan& plan);
  void cancel() noexcept;
  void reset() noexcept;

  [[nodiscard]] model_loading_status status() const noexcept {
    return status_.load(std::memory_order_acquire);
  }
  [[nodiscard]] granit::result result() const noexcept { return result_; }
  [[nodiscard]] const std::string& diagnostic() const noexcept { return diagnostic_; }
  [[nodiscard]] gltf::document_load_progress progress() const noexcept {
    return document_.progress();
  }

private:
  void fail(granit::result result, std::string diagnostic);

  gltf::document_loader document_;
  gltf::scene scene_;
  gpu_scene_plan plan_;
  std::atomic<model_loading_status> status_{model_loading_status::idle};
  std::atomic_bool cancel_requested_{};
  granit::result result_{granit::result::success};
  std::string diagnostic_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_LOADING_SESSION_H_
