// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/app/model_load_operation.h"

#include <chrono>
#include <future>
#include <new>
#include <utility>

namespace granit::example::model_viewer {
namespace {

model_load_error map_asset_error(assets::asset_error error) noexcept {
  switch (error) {
  case assets::asset_error::none:
    return model_load_error::none;
  case assets::asset_error::invalid_location:
    return model_load_error::invalid_location;
  case assets::asset_error::io_error:
  case assets::asset_error::transport_error:
  case assets::asset_error::source_not_registered:
    return model_load_error::document_read;
  case assets::asset_error::dependency_read:
    return model_load_error::resource_read;
  case assets::asset_error::invalid_data:
  case assets::asset_error::loader_not_registered:
    return model_load_error::invalid_document;
  case assets::asset_error::out_of_memory:
    return model_load_error::out_of_memory;
  case assets::asset_error::cancelled:
    return model_load_error::cancelled;
  case assets::asset_error::internal:
    return model_load_error::import;
  }
  return model_load_error::invalid_document;
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

struct model_load_operation::prepare_state {
  granit::result result{granit::result::not_ready};
  bool complete{};
#if !defined(__EMSCRIPTEN__)
  std::future<granit::result> operation;
#endif
};

model_load_operation::model_load_operation() = default;

model_load_operation::~model_load_operation() { reset(); }

bool model_load_operation::start(assets::asset_manager& assets,
                                 assets::asset_location model) noexcept {
  const auto current = status();
  if (current == model_load_status::loading_assets || current == model_load_status::preparing) {
    return false;
  }
  reset();
  scene_asset_ = assets.load<gltf::scene>(std::move(model));
  if (!scene_asset_.valid())
    return false;
  status_.store(model_load_status::loading_assets, std::memory_order_release);
  return true;
}

void model_load_operation::poll_assets() {
  if (status() != model_load_status::loading_assets)
    return;
  switch (scene_asset_.status()) {
  case assets::asset_status::ready:
    status_.store(model_load_status::assets_ready, std::memory_order_release);
    break;
  case assets::asset_status::failed:
    fail(map_asset_error(scene_asset_.error()),
         scene_asset_.error() == assets::asset_error::out_of_memory
             ? granit::result::out_of_memory
             : granit::result::invalid_argument,
         scene_asset_.diagnostic());
    break;
  case assets::asset_status::cancelled:
    error_ = model_load_error::cancelled;
    result_ = granit::result::cancelled;
    status_.store(model_load_status::cancelled, std::memory_order_release);
    break;
  case assets::asset_status::queued:
  case assets::asset_status::reading:
  case assets::asset_status::discovering_dependencies:
  case assets::asset_status::loading_dependencies:
  case assets::asset_status::decoding:
    break;
  }
}

granit::result model_load_operation::begin_prepare(gltf::import_progress_callback progress,
                                                   void* progress_user_data) noexcept {
  if (status() != model_load_status::assets_ready || prepare_)
    return granit::result::invalid_argument;
  status_.store(model_load_status::preparing, std::memory_order_release);
  try {
    auto candidate = std::make_unique<prepare_state>();
#if defined(__EMSCRIPTEN__)
    candidate->result = prepare(progress, progress_user_data);
    candidate->complete = true;
#else
    candidate->operation = std::async(std::launch::async, [this, progress, progress_user_data] {
      return prepare(progress, progress_user_data);
    });
#endif
    prepare_ = std::move(candidate);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    status_.store(model_load_status::assets_ready, std::memory_order_release);
    return granit::result::out_of_memory;
  } catch (...) {
    status_.store(model_load_status::assets_ready, std::memory_order_release);
    return granit::result::initialization_failed;
  }
}

granit::result model_load_operation::poll_prepare() noexcept {
  if (!prepare_)
    return granit::result::invalid_argument;
#if defined(__EMSCRIPTEN__)
  return prepare_->complete ? prepare_->result : granit::result::not_ready;
#else
  if (prepare_->complete)
    return prepare_->result;
  if (prepare_->operation.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready)
    return granit::result::not_ready;
  try {
    prepare_->result = prepare_->operation.get();
  } catch (const std::bad_alloc&) {
    prepare_->result = granit::result::out_of_memory;
  } catch (...) {
    prepare_->result = granit::result::internal;
  }
  prepare_->complete = true;
  return prepare_->result;
#endif
}

granit::result model_load_operation::prepare(gltf::import_progress_callback progress,
                                             void* progress_user_data) {
  if (status() != model_load_status::preparing)
    return granit::result::invalid_argument;
  progress_context context{.cancel_requested = &cancel_requested_,
                           .callback = progress,
                           .user_data = progress_user_data};
  const auto loaded_scene = scene_asset_.value();
  if (!loaded_scene) {
    fail(model_load_error::import, granit::result::internal, "无法取得已加载的 CPU Scene");
    return result_;
  }
  if (!report_progress({gltf::import_stage::document, 1, 1}, &context)) {
    error_ = model_load_error::cancelled;
    result_ = granit::result::cancelled;
    diagnostic_.clear();
    status_.store(model_load_status::cancelled, std::memory_order_release);
    return result_;
  }
  gltf::scene scene = *loaded_scene;

  gltf_rendering::scene_plan plan;
  const auto planned = gltf_rendering::build_scene_plan(scene, plan);
  if (planned != gltf_rendering::scene_plan_error::none) {
    fail(model_load_error::gpu_plan,
         planned == gltf_rendering::scene_plan_error::out_of_memory
             ? granit::result::out_of_memory
             : granit::result::invalid_argument,
         "生成 glTF Scene GPU 资源 计划失败");
    return result_;
  }
  scene_ = std::move(scene);
  plan_ = std::move(plan);
  error_ = model_load_error::none;
  result_ = granit::result::success;
  status_.store(model_load_status::ready, std::memory_order_release);
  return result_;
}

bool model_load_operation::take(gltf::scene& scene, gltf_rendering::scene_plan& plan) {
  if (status() != model_load_status::ready)
    return false;
  scene = std::move(scene_);
  plan = std::move(plan_);
  status_.store(model_load_status::idle, std::memory_order_release);
  return true;
}

void model_load_operation::cancel() noexcept {
  cancel_requested_.store(true, std::memory_order_release);
  if (status() == model_load_status::loading_assets) {
    scene_asset_.cancel();
    error_ = model_load_error::cancelled;
    result_ = granit::result::cancelled;
    diagnostic_.clear();
    status_.store(model_load_status::cancelled, std::memory_order_release);
  }
}

void model_load_operation::reset() noexcept {
  cancel_requested_.store(true, std::memory_order_release);
#if !defined(__EMSCRIPTEN__)
  if (prepare_ && prepare_->operation.valid()) {
    try {
      static_cast<void>(prepare_->operation.get());
    } catch (...) {
    }
  }
#endif
  prepare_.reset();
  scene_asset_ = {};
  scene_ = {};
  plan_ = {};
  cancel_requested_.store(false, std::memory_order_release);
  error_ = model_load_error::none;
  result_ = granit::result::success;
  diagnostic_.clear();
  status_.store(model_load_status::idle, std::memory_order_release);
}

void model_load_operation::fail(model_load_error error, granit::result result,
                                std::string diagnostic) {
  error_ = error;
  result_ = result;
  diagnostic_ = std::move(diagnostic);
  status_.store(model_load_status::failed, std::memory_order_release);
}

} // namespace granit::example::model_viewer
