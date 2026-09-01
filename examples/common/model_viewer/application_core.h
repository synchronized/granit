// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_APPLICATION_CORE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_APPLICATION_CORE_H_

#include "gltf/loader.h"
#include "model_viewer/gpu_scene.h"
#include "model_viewer/performance_history.h"
#include "model_viewer/viewer_state.h"

#include <span>
#include <string>

namespace granit::example::model_viewer {

enum class application_phase {
  platform_ready,
  renderer_pending,
  asset_loading,
  gpu_upload,
  ready,
  failed,
};

class application_core {
public:
  [[nodiscard]] granit::result begin_renderer() noexcept;
  [[nodiscard]] granit::result renderer_ready() noexcept;
  [[nodiscard]] granit::result load_asset(std::span<const std::byte> bytes,
                                          const gltf::resource_resolver* resolver);
  [[nodiscard]] granit::result accept_scene(gltf::scene scene);
  [[nodiscard]] granit::result upload(granit_renderer renderer);
  void fail(granit::result result, std::string diagnostic);
  void reset() noexcept;

  [[nodiscard]] application_phase phase() const noexcept { return phase_; }
  [[nodiscard]] granit::result failure_result() const noexcept { return failure_result_; }
  [[nodiscard]] const std::string& diagnostic() const noexcept { return diagnostic_; }
  [[nodiscard]] gltf::scene& cpu_scene() noexcept { return cpu_scene_; }
  [[nodiscard]] const gltf::scene& cpu_scene() const noexcept { return cpu_scene_; }
  [[nodiscard]] gpu_scene& scene_gpu() noexcept { return gpu_scene_; }
  [[nodiscard]] viewer_state& state() noexcept { return state_; }
  [[nodiscard]] performance_history& performance() noexcept { return performance_; }

private:
  application_phase phase_{application_phase::platform_ready};
  granit::result failure_result_{granit::result::success};
  std::string diagnostic_;
  gltf::scene cpu_scene_;
  gpu_scene gpu_scene_;
  viewer_state state_;
  performance_history performance_;
};

} // namespace granit::example::model_viewer

#endif
