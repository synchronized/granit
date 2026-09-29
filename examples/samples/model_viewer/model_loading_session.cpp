// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/model_loading_session.h"

#include <utility>

namespace granit::example::model_viewer {
namespace {

model_loading_error map_asset_error(assets::asset_error error) noexcept {
  switch (error) {
  case assets::asset_error::none:
    return model_loading_error::none;
  case assets::asset_error::invalid_location:
    return model_loading_error::invalid_location;
  case assets::asset_error::io_error:
  case assets::asset_error::transport_error:
  case assets::asset_error::source_not_registered:
    return model_loading_error::document_read;
  case assets::asset_error::dependency_read:
    return model_loading_error::resource_read;
  case assets::asset_error::invalid_data:
  case assets::asset_error::loader_not_registered:
    return model_loading_error::invalid_document;
  case assets::asset_error::out_of_memory:
    return model_loading_error::out_of_memory;
  case assets::asset_error::cancelled:
    return model_loading_error::cancelled;
  case assets::asset_error::internal:
    return model_loading_error::import;
  }
  return model_loading_error::invalid_document;
}

struct progress_context {
  std::atomic_bool* cancel_requested{};
  gltf::import_progress_callback callback{};
  void* user_data{};
};

bool report_progress(const gltf::import_progress& progress, void* user_data) {
  const auto& context = *static_cast<const progress_context*>(user_data);
  if (context.cancel_requested->load(std::memory_order_acquire))
    return false;
  if (context.callback != nullptr && !context.callback(progress, context.user_data))
    return false;
  return !context.cancel_requested->load(std::memory_order_acquire);
}

} // namespace

bool model_loading_session::start(assets::asset_manager& assets,
                                  assets::asset_location model) noexcept {
  const auto current = status();
  if (current == model_loading_status::loading_assets ||
      current == model_loading_status::preparing) {
    return false;
  }
  reset();
  scene_asset_ = assets.load<gltf::scene>(std::move(model));
  if (!scene_asset_.valid())
    return false;
  status_.store(model_loading_status::loading_assets, std::memory_order_release);
  return true;
}

void model_loading_session::poll() {
  if (status() != model_loading_status::loading_assets)
    return;
  switch (scene_asset_.status()) {
  case assets::asset_status::ready:
    status_.store(model_loading_status::assets_ready, std::memory_order_release);
    break;
  case assets::asset_status::failed:
    fail(map_asset_error(scene_asset_.error()),
         scene_asset_.error() == assets::asset_error::out_of_memory
             ? granit::result::out_of_memory
             : granit::result::invalid_argument,
         scene_asset_.diagnostic());
    break;
  case assets::asset_status::cancelled:
    error_ = model_loading_error::cancelled;
    result_ = granit::result::cancelled;
    status_.store(model_loading_status::cancelled, std::memory_order_release);
    break;
  case assets::asset_status::queued:
  case assets::asset_status::reading:
  case assets::asset_status::discovering_dependencies:
  case assets::asset_status::loading_dependencies:
  case assets::asset_status::decoding:
    break;
  }
}

granit::result model_loading_session::prepare(gltf::import_progress_callback progress,
                                              void* progress_user_data) {
  if (status() != model_loading_status::assets_ready)
    return granit::result::invalid_argument;
  status_.store(model_loading_status::preparing, std::memory_order_release);
  progress_context context{.cancel_requested = &cancel_requested_,
                           .callback = progress,
                           .user_data = progress_user_data};
  const auto loaded_scene = scene_asset_.value();
  if (!loaded_scene) {
    fail(model_loading_error::import, granit::result::internal, "无法取得已加载的 CPU Scene");
    return result_;
  }
  if (!report_progress({gltf::import_stage::document, 1, 1}, &context)) {
    error_ = model_loading_error::cancelled;
    result_ = granit::result::cancelled;
    diagnostic_.clear();
    status_.store(model_loading_status::cancelled, std::memory_order_release);
    return result_;
  }
  gltf::scene scene = *loaded_scene;

  gltf_rendering::gpu_scene_plan plan;
  const auto planned = gltf_rendering::build_gpu_scene_plan(scene, plan);
  if (planned != gltf_rendering::gpu_scene_plan_error::none) {
    fail(model_loading_error::gpu_plan,
         planned == gltf_rendering::gpu_scene_plan_error::out_of_memory
             ? granit::result::out_of_memory
             : granit::result::invalid_argument,
         "生成 GPU Scene 计划失败");
    return result_;
  }
  scene_ = std::move(scene);
  plan_ = std::move(plan);
  error_ = model_loading_error::none;
  result_ = granit::result::success;
  status_.store(model_loading_status::ready, std::memory_order_release);
  return result_;
}

bool model_loading_session::take(gltf::scene& scene, gltf_rendering::gpu_scene_plan& plan) {
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
    scene_asset_.cancel();
    error_ = model_loading_error::cancelled;
    result_ = granit::result::cancelled;
    diagnostic_.clear();
    status_.store(model_loading_status::cancelled, std::memory_order_release);
  }
}

void model_loading_session::reset() noexcept {
  scene_asset_ = {};
  scene_ = {};
  plan_ = {};
  cancel_requested_.store(false, std::memory_order_release);
  error_ = model_loading_error::none;
  result_ = granit::result::success;
  diagnostic_.clear();
  status_.store(model_loading_status::idle, std::memory_order_release);
}

void model_loading_session::fail(model_loading_error error, granit::result result,
                                 std::string diagnostic) {
  error_ = error;
  result_ = result;
  diagnostic_ = std::move(diagnostic);
  status_.store(model_loading_status::failed, std::memory_order_release);
}

} // namespace granit::example::model_viewer
