// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/application_core.h"

#include <new>
#include <utility>

namespace granit::example::model_viewer {

granit::result application_core::begin_renderer() noexcept {
  if (phase_ != application_phase::platform_ready)
    return granit::result::invalid_argument;
  phase_ = application_phase::renderer_pending;
  return granit::result::success;
}

granit::result application_core::renderer_ready() noexcept {
  if (phase_ != application_phase::renderer_pending)
    return granit::result::invalid_argument;
  phase_ = application_phase::asset_loading;
  return granit::result::success;
}

granit::result application_core::load_asset(std::span<const std::byte> bytes,
                                            const gltf::resource_resolver* resolver) {
  if (phase_ != application_phase::asset_loading)
    return granit::result::invalid_argument;
  gltf::scene candidate;
  const auto loaded = gltf::load(bytes, resolver, candidate);
  if (!loaded) {
    fail(granit::result::invalid_argument, loaded.diagnostic);
    return failure_result_;
  }
  return accept_scene(std::move(candidate));
}

granit::result application_core::accept_scene(gltf::scene scene) {
  if (phase_ != application_phase::asset_loading)
    return granit::result::invalid_argument;
  try {
    state_.reset(scene);
    cpu_scene_ = std::move(scene);
    phase_ = application_phase::gpu_upload;
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    fail(granit::result::out_of_memory, "模型查看器 CPU Scene 分配失败");
    return failure_result_;
  }
}

granit::result application_core::upload(granit_renderer renderer) {
  if (phase_ != application_phase::gpu_upload)
    return granit::result::invalid_argument;
  const auto result = gpu_scene_.initialize(renderer, cpu_scene_);
  if (granit::failed(result)) {
    fail(result, "模型查看器 GPU Scene 上传失败");
    return result;
  }
  phase_ = application_phase::ready;
  return granit::result::success;
}

void application_core::fail(granit::result result, std::string diagnostic) {
  gpu_scene_.reset();
  failure_result_ = result;
  diagnostic_ = std::move(diagnostic);
  phase_ = application_phase::failed;
}

void application_core::reset() noexcept {
  gpu_scene_.reset();
  cpu_scene_ = {};
  state_ = {};
  performance_.clear();
  diagnostic_.clear();
  failure_result_ = granit::result::success;
  phase_ = application_phase::platform_ready;
}

} // namespace granit::example::model_viewer
