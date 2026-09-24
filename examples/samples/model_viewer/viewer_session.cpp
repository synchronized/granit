// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_session.h"

#include <utility>

namespace granit::example::model_viewer {

granit::result viewer_session::begin_renderer() noexcept { return core_.begin_renderer(); }

granit::result viewer_session::renderer_ready() noexcept { return core_.renderer_ready(); }

bool viewer_session::start_loading(assets::asset_system& assets, assets::asset_key model) {
  return loading_.start(assets, model);
}

void viewer_session::poll_loading() {
  loading_.poll();
  if (loading_.status() == model_loading_status::failed)
    fail_from_loading();
}

granit::result viewer_session::prepare_scene(gltf::import_progress_callback progress,
                                             void* progress_user_data) {
  auto result = loading_.prepare(progress, progress_user_data);
  if (result.failed()) {
    fail_from_loading();
    return result;
  }
  gltf::scene scene;
  gpu_scene_plan plan;
  if (!loading_.take(scene, plan)) {
    result = granit::result::internal;
    core_.fail(result, "无法取得已准备的模型资源");
    return result;
  }
  result = core_.accept_scene(std::move(scene), std::move(plan));
  if (result.failed())
    core_.fail(result, "Viewer Core 无法接收已准备的模型资源");
  return result;
}

void viewer_session::cancel_loading() noexcept { loading_.cancel(); }

void viewer_session::reset() noexcept {
  loading_.reset();
  core_.reset();
}

model_loading_status viewer_session::loading_status() const noexcept { return loading_.status(); }

model_loading_error viewer_session::loading_error() const noexcept { return loading_.error(); }

granit::result viewer_session::loading_result() const noexcept { return loading_.result(); }

const std::string& viewer_session::loading_diagnostic() const noexcept {
  return loading_.diagnostic();
}

gltf::document_load_progress viewer_session::loading_progress() const noexcept {
  return loading_.progress();
}

void viewer_session::fail_from_loading() { core_.fail(loading_.result(), loading_.diagnostic()); }

} // namespace granit::example::model_viewer
