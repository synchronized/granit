// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/model_viewer_runtime.h"

#include <utility>

namespace granit::example::model_viewer {

model_viewer_runtime::model_viewer_runtime(application_core& core,
                                           model_loading_session& loading) noexcept
    : core_(&core), loading_(&loading) {}

granit::result model_viewer_runtime::begin_renderer() noexcept { return core_->begin_renderer(); }

granit::result model_viewer_runtime::renderer_ready() noexcept { return core_->renderer_ready(); }

bool model_viewer_runtime::start_loading(assets::asset_system& assets, assets::asset_key model) {
  return loading_->start(assets, model);
}

void model_viewer_runtime::poll_loading() {
  loading_->poll();
  if (loading_->status() == model_loading_status::failed)
    fail_from_loading();
}

granit::result model_viewer_runtime::prepare_scene(gltf::import_progress_callback progress,
                                                   void* progress_user_data) {
  auto result = loading_->prepare(progress, progress_user_data);
  if (result.failed()) {
    fail_from_loading();
    return result;
  }
  gltf::scene scene;
  gpu_scene_plan plan;
  if (!loading_->take(scene, plan)) {
    result = granit::result::internal;
    core_->fail(result, "无法取得已准备的模型资源");
    return result;
  }
  result = core_->accept_scene(std::move(scene), std::move(plan));
  if (result.failed())
    core_->fail(result, "Viewer Core 无法接收已准备的模型资源");
  return result;
}

void model_viewer_runtime::cancel_loading() noexcept { loading_->cancel(); }

void model_viewer_runtime::reset() noexcept {
  loading_->reset();
  core_->reset();
}

model_loading_status model_viewer_runtime::loading_status() const noexcept {
  return loading_->status();
}

granit::result model_viewer_runtime::loading_result() const noexcept { return loading_->result(); }

const std::string& model_viewer_runtime::loading_diagnostic() const noexcept {
  return loading_->diagnostic();
}

gltf::document_load_progress model_viewer_runtime::loading_progress() const noexcept {
  return loading_->progress();
}

void model_viewer_runtime::fail_from_loading() {
  core_->fail(loading_->result(), loading_->diagnostic());
}

} // namespace granit::example::model_viewer
