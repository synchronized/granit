// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_LOAD_OPERATION_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_LOAD_OPERATION_H_

#include "assets/asset_manager.h"
#include "gltf/importer.h"
#include "gltf/scene.h"
#include "gltf_rendering/scene_resources.h"

#include <atomic>
#include <memory>
#include <string>

namespace granit::example::model_viewer {

enum class model_load_status {
  idle,
  loading_assets,
  assets_ready,
  preparing,
  ready,
  failed,
  cancelled,
};

enum class model_load_error {
  none,
  invalid_location,
  document_read,
  invalid_document,
  resource_read,
  out_of_memory,
  import,
  gpu_plan,
  cancelled,
};

/** 统一模型文档、外部资源、CPU Scene 与 GPU 创建计划的加载状态。 */
class model_load_operation final {
public:
  model_load_operation();
  ~model_load_operation();
  model_load_operation(const model_load_operation&) = delete;
  model_load_operation& operator=(const model_load_operation&) = delete;

  [[nodiscard]] bool start(assets::asset_manager& assets, assets::asset_location model) noexcept;
  void poll_assets();
  [[nodiscard]] granit::result
  begin_prepare(gltf::import_progress_callback progress = nullptr,
                void* progress_user_data = nullptr) noexcept;
  /** 返回 not_ready 表示 CPU Scene 与 GPU 计划仍在准备。 */
  [[nodiscard]] granit::result poll_prepare() noexcept;
  [[nodiscard]] bool take(gltf::scene& scene, gltf_rendering::scene_plan& plan);
  void cancel() noexcept;
  void reset() noexcept;

  [[nodiscard]] model_load_status status() const noexcept {
    return status_.load(std::memory_order_acquire);
  }
  [[nodiscard]] model_load_error error() const noexcept { return error_; }
  [[nodiscard]] granit::result result() const noexcept { return result_; }
  [[nodiscard]] const std::string& diagnostic() const noexcept { return diagnostic_; }
  [[nodiscard]] assets::asset_progress progress() const noexcept { return scene_asset_.progress(); }

private:
  struct prepare_state;

  [[nodiscard]] granit::result prepare(gltf::import_progress_callback progress,
                                       void* progress_user_data);
  void fail(model_load_error error, granit::result result, std::string diagnostic);

  assets::asset_handle<gltf::scene> scene_asset_;
  gltf::scene scene_;
  gltf_rendering::scene_plan plan_;
  std::atomic<model_load_status> status_{model_load_status::idle};
  std::atomic_bool cancel_requested_{};
  model_load_error error_{model_load_error::none};
  granit::result result_{granit::result::success};
  std::string diagnostic_;
  std::unique_ptr<prepare_state> prepare_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MODEL_LOAD_OPERATION_H_
