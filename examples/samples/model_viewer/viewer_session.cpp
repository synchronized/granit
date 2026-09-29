// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_session.h"

#include <utility>

namespace granit::example::model_viewer {

granit::result viewer_session::begin_renderer() noexcept { return core_.begin_renderer(); }

granit::result viewer_session::renderer_ready() noexcept { return core_.renderer_ready(); }

bool viewer_session::start_loading(assets::asset_manager& assets,
                                   assets::asset_location model) noexcept {
  return load_operation_.start(assets, std::move(model));
}

void viewer_session::poll_loading() {
  load_operation_.poll_assets();
  if (load_operation_.status() == model_load_status::failed)
    fail_from_loading();
}

granit::result viewer_session::begin_scene_prepare(gltf::import_progress_callback progress,
                                                   void* progress_user_data) noexcept {
  return load_operation_.begin_prepare(progress, progress_user_data);
}

granit::result viewer_session::poll_scene_prepare() noexcept {
  auto result = load_operation_.poll_prepare();
  if (result == granit::result::not_ready)
    return result;
  if (result.failed()) {
    if (load_operation_.result().failed())
      fail_from_loading();
    else
      core_.fail(result, "准备模型资源时发生未处理错误");
    return result;
  }
  gltf::scene scene;
  gltf_rendering::scene_plan plan;
  if (!load_operation_.take(scene, plan)) {
    result = granit::result::internal;
    core_.fail(result, "无法取得已准备的模型资源");
    return result;
  }
  result = core_.accept_scene(std::move(scene), std::move(plan));
  if (result.failed())
    core_.fail(result, "Viewer Core 无法接收已准备的模型资源");
  return result;
}

void viewer_session::cancel_loading() noexcept { load_operation_.cancel(); }

void viewer_session::reset() noexcept {
  load_operation_.reset();
  core_.reset();
}

model_load_status viewer_session::loading_status() const noexcept { return load_operation_.status(); }

model_load_error viewer_session::loading_error() const noexcept { return load_operation_.error(); }

granit::result viewer_session::loading_result() const noexcept { return load_operation_.result(); }

const std::string& viewer_session::loading_diagnostic() const noexcept {
  return load_operation_.diagnostic();
}

assets::asset_progress viewer_session::loading_progress() const noexcept {
  return load_operation_.progress();
}

void viewer_session::fail_from_loading() { core_.fail(load_operation_.result(), load_operation_.diagnostic()); }

} // namespace granit::example::model_viewer
