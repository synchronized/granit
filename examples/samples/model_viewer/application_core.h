// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_APPLICATION_CORE_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_APPLICATION_CORE_H_

#include "gltf/importer.h"
#include "model_viewer/viewer_document.h"

#include <granit/pipeline/render_pipeline.hpp>

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

/** Document 生成的单帧不可变数据；GPU Snapshot 由渲染线程创建。 */
struct viewer_frame {
  granit::scene_view view;
  granit::scene_directional_light directional_light;
  std::uint32_t width{};
  std::uint32_t height{};
  float exposure_ev{};
  float environment_intensity{};
  float environment_rotation_radians{};
  granit::clear_color_value clear_color{0.0F, 0.0F, 0.0F, 1.0F};
};

class application_core {
public:
  [[nodiscard]] granit::result begin_renderer() noexcept;
  [[nodiscard]] granit::result renderer_ready() noexcept;
  [[nodiscard]] granit::result accept_scene(gltf::scene scene);
  /** 接收已经在资产线程完成打包的 CPU Scene 与 GPU 创建计划。 */
  [[nodiscard]] granit::result accept_scene(gltf::scene scene, gltf_rendering::scene_plan plan);
  [[nodiscard]] granit::result prepare_upload(gltf::scene& scene,
                                              gltf_rendering::scene_plan& plan) const;
  [[nodiscard]] granit::result complete_upload(float recommended_exposure_ev,
                                               float environment_intensity) noexcept;
  [[nodiscard]] granit::result tick(const viewer_document_update& input, viewer_frame& output);
  [[nodiscard]] granit::result
  update_material(std::uint32_t material_index,
                  const gltf_rendering::material_factor_update& edit) noexcept;
  void fail(granit::result result, std::string diagnostic);
  void reset() noexcept;

  [[nodiscard]] application_phase phase() const noexcept { return phase_; }
  [[nodiscard]] granit::result failure_result() const noexcept { return failure_result_; }
  [[nodiscard]] const std::string& diagnostic() const noexcept { return diagnostic_; }
  [[nodiscard]] gltf::scene& cpu_scene() noexcept { return document_.scene(); }
  [[nodiscard]] const gltf::scene& cpu_scene() const noexcept { return document_.scene(); }
  [[nodiscard]] viewer_state& state() noexcept { return document_.state(); }
  [[nodiscard]] performance_history& performance() noexcept { return document_.performance(); }

private:
  application_phase phase_{application_phase::platform_ready};
  granit::result failure_result_{granit::result::success};
  std::string diagnostic_;
  viewer_document document_;
  gltf_rendering::scene_plan gpu_plan_;
};

} // namespace granit::example::model_viewer

#endif
