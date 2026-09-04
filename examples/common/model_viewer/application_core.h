// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_APPLICATION_CORE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_APPLICATION_CORE_H_

#include "gltf/loader.h"
#include "model_viewer/environment_resources.h"
#include "model_viewer/frame_canvas_data.h"
#include "model_viewer/gpu_scene.h"
#include "model_viewer/performance_history.h"
#include "model_viewer/viewer_state.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace granit::example::model_viewer {

enum class application_phase {
  platform_ready,
  renderer_pending,
  asset_loading,
  gpu_upload,
  ready,
  failed,
};

/** 平台壳在一帧开始时提交的后端无关输入。 */
struct application_tick_input {
  viewer_input input;
  viewer_change change;
  std::uint32_t width{};
  std::uint32_t height{};
  std::optional<performance_sample> performance;
};

/** Core 生成的单帧不可变提交包；其数组和环境数据不借用下一帧可变状态。 */
struct frame_packet {
  frame_canvas_data canvas;
  granit::scene_snapshot snapshot;
  granit_render_pipeline_environment environment = GRANIT_RENDER_PIPELINE_ENVIRONMENT_INIT;
  std::vector<granit_render_pipeline_draw_binding> draw_bindings;
  std::uint32_t width{};
  std::uint32_t height{};
  float exposure_ev{};
  granit_clear_color_value clear_color{0.0F, 0.0F, 0.0F, 1.0F};

  /** 在消费线程生成只借用当前提交包的渲染描述。 */
  [[nodiscard]] granit_render_pipeline_render_desc
  render_desc(granit_texture_view output, granit_texture_format output_format,
              granit_frame frame = GRANIT_NULL_HANDLE,
              granit_canvas_draw_list canvas_list = GRANIT_NULL_HANDLE) const noexcept;
};

class application_core {
public:
  [[nodiscard]] granit::result begin_renderer() noexcept;
  [[nodiscard]] granit::result renderer_ready() noexcept;
  [[nodiscard]] granit::result load_asset(std::span<const std::byte> bytes,
                                          const gltf::resource_resolver* resolver);
  [[nodiscard]] granit::result accept_scene(gltf::scene scene);
  /** 上传场景；environment_bytes 为空时使用内置摄影棚环境，否则解析 GRENV v2。 */
  [[nodiscard]] granit::result upload(granit_renderer renderer,
                                      std::span<const std::byte> environment_bytes = {},
                                      float sampler_anisotropy = 8.0F,
                                      gpu_scene_upload_callback progress = nullptr,
                                      void* progress_user_data = nullptr);
  /** 按新采样质量事务式重建 GPU Scene；环境资源与查看器状态保持不变。 */
  [[nodiscard]] granit::result reupload_scene(granit_renderer renderer, float sampler_anisotropy);
  [[nodiscard]] granit::result tick(const application_tick_input& input, frame_packet& output);
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
  environment_resources environment_;
  viewer_state state_;
  performance_history performance_;
  bool camera_initialized_{};
};

} // namespace granit::example::model_viewer

#endif
