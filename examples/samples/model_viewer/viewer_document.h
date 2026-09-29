// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_DOCUMENT_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_DOCUMENT_H_

#include "gltf/scene.h"
#include "gltf_rendering/scene_plan.h"
#include "model_viewer/performance_history.h"
#include "model_viewer/viewer_state.h"

#include <granit/pipeline/scene.hpp>
#include <granit/renderer/render_target.hpp>

#include <optional>

namespace granit::example::model_viewer {

/** 平台壳在一帧开始时提交的后端无关输入。 */
struct viewer_document_update {
  camera::orbit_camera_input input;
  viewer_change change;
  std::uint32_t width{};
  std::uint32_t height{};
  std::optional<performance_sample> performance;
};

/** Viewer Document 生成的单帧业务数据，不包含 GPU 资源。 */
struct viewer_document_frame {
  granit::scene_view view;
  granit::scene_directional_light directional_light;
  float exposure_ev{};
  float environment_intensity{};
  float environment_rotation_radians{};
  granit::clear_color_value clear_color{0.0F, 0.0F, 0.0F, 1.0F};
};

/** 保存当前模型及其查看状态；不持有 Renderer、Window 或执行器。 */
class viewer_document final {
public:
  void assign(gltf::scene scene);
  void clear() noexcept;

  [[nodiscard]] viewer_state_error apply(const viewer_change& change) {
    return state_.apply(scene_, change);
  }
  [[nodiscard]] granit::result update(const viewer_document_update& input,
                                      const gltf_rendering::scene_plan& plan,
                                      viewer_document_frame& output);

  [[nodiscard]] gltf::scene& scene() noexcept { return scene_; }
  [[nodiscard]] const gltf::scene& scene() const noexcept { return scene_; }
  [[nodiscard]] viewer_state& state() noexcept { return state_; }
  [[nodiscard]] const viewer_state& state() const noexcept { return state_; }
  [[nodiscard]] performance_history& performance() noexcept { return performance_; }
  [[nodiscard]] const performance_history& performance() const noexcept { return performance_; }

private:
  gltf::scene scene_;
  viewer_state state_;
  performance_history performance_;
  bool camera_initialized_{};
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_DOCUMENT_H_
