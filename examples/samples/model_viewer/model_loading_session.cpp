// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/model_loading_session.h"

#include <utility>

namespace granit::example::model_viewer {
namespace {

struct progress_context {
  std::atomic_bool* cancel_requested{};
  gltf::import_progress_callback callback{};
  void* user_data{};
};

bool report_progress(const gltf::import_progress& progress, void* user_data) {
  const auto& context = *static_cast<const progress_context*>(user_data);
  if (context.cancel_requested->load(std::memory_order_acquire))
    return false;
  return context.callback == nullptr || context.callback(progress, context.user_data);
}

} // namespace

bool model_loading_session::start(assets::asset_system& assets, assets::asset_key model) {
  const auto current = status();
  if (current == model_loading_status::loading_assets ||
      current == model_loading_status::preparing) {
    return false;
  }
  reset();
  if (!document_.start(assets, model))
    return false;
  status_.store(model_loading_status::loading_assets, std::memory_order_release);
  return true;
}

void model_loading_session::poll() {
  if (status() != model_loading_status::loading_assets)
    return;
  document_.poll();
  switch (document_.status()) {
  case gltf::document_load_status::ready:
    status_.store(model_loading_status::assets_ready, std::memory_order_release);
    break;
  case gltf::document_load_status::failed:
    fail(document_.error() == gltf::document_load_error::out_of_memory
             ? granit::result::out_of_memory
             : granit::result::invalid_argument,
         std::string{document_.diagnostic()});
    break;
  case gltf::document_load_status::cancelled:
    result_ = granit::result::cancelled;
    status_.store(model_loading_status::cancelled, std::memory_order_release);
    break;
  case gltf::document_load_status::idle:
  case gltf::document_load_status::loading_document:
  case gltf::document_load_status::loading_resources:
    break;
  }
}

granit::result model_loading_session::prepare(gltf::import_progress_callback progress,
                                              void* progress_user_data) {
  if (status() != model_loading_status::assets_ready)
    return granit::result::invalid_argument;
  status_.store(model_loading_status::preparing, std::memory_order_release);
  progress_context context{
      .cancel_requested = &cancel_requested_, .callback = progress, .user_data = progress_user_data};
  gltf::scene scene;
  const auto loaded = gltf::import_scene(document_.document(), &document_.resolver(), scene,
                                         report_progress, &context);
  if (!loaded) {
    if (loaded.error == gltf::import_error::cancelled) {
      result_ = granit::result::cancelled;
      diagnostic_.clear();
      status_.store(model_loading_status::cancelled, std::memory_order_release);
      return result_;
    }
    fail(granit::result::invalid_argument, loaded.diagnostic);
    return result_;
  }

  gpu_scene_plan plan;
  const auto planned = build_gpu_scene_plan(scene, plan);
  if (planned != gpu_scene_plan_error::none) {
    fail(planned == gpu_scene_plan_error::out_of_memory ? granit::result::out_of_memory
                                                         : granit::result::invalid_argument,
         "生成 GPU Scene 计划失败");
    return result_;
  }
  scene_ = std::move(scene);
  plan_ = std::move(plan);
  result_ = granit::result::success;
  status_.store(model_loading_status::ready, std::memory_order_release);
  return result_;
}

bool model_loading_session::take(gltf::scene& scene, gpu_scene_plan& plan) {
  if (status() != model_loading_status::ready)
    return false;
  scene = std::move(scene_);
  plan = std::move(plan_);
  status_.store(model_loading_status::idle, std::memory_order_release);
  return true;
}

void model_loading_session::cancel() noexcept {
  cancel_requested_.store(true, std::memory_order_release);
  if (status() == model_loading_status::loading_assets) {
    document_.cancel();
    result_ = granit::result::cancelled;
    diagnostic_.clear();
    status_.store(model_loading_status::cancelled, std::memory_order_release);
  }
}

void model_loading_session::reset() noexcept {
  document_.reset();
  scene_ = {};
  plan_ = {};
  cancel_requested_.store(false, std::memory_order_release);
  result_ = granit::result::success;
  diagnostic_.clear();
  status_.store(model_loading_status::idle, std::memory_order_release);
}

void model_loading_session::fail(granit::result result, std::string diagnostic) {
  result_ = result;
  diagnostic_ = std::move(diagnostic);
  status_.store(model_loading_status::failed, std::memory_order_release);
}

} // namespace granit::example::model_viewer
