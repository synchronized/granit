// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_RENDERER_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_RENDERER_H_

#include "gltf_rendering/scene_resources.h"
#include "model_viewer/rendering/pipeline_prepare.h"
#include "model_viewer/rendering/render_execution.h"

#include <granit/granit.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/environment_map.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/window.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace granit::example::model_viewer {

struct render_quality_change_result {
  bool scene_reuploaded{};
};

/** 同步 GPU 运行时；调用方保证所有 GPU 操作在选定执行器线程中串行执行。 */
class viewer_renderer final {
public:
  viewer_renderer();
  ~viewer_renderer();
  viewer_renderer(const viewer_renderer&) = delete;
  viewer_renderer& operator=(const viewer_renderer&) = delete;

  [[nodiscard]] granit::result initialize_renderer(const granit::renderer_desc& desc) noexcept;
  /** Renderer 进入 ready 后查询稳定属性；异步后端由平台循环决定调用时机。 */
  [[nodiscard]] granit::result complete_renderer_initialization() noexcept;
  [[nodiscard]] granit::result initialize_presentation(granit::surface surface,
                                                       const granit::swapchain_desc& desc,
                                                       bool enable_ui) noexcept;
  [[nodiscard]] granit::result process_renderer_events() noexcept;
  [[nodiscard]] granit::result
  query_renderer_status(granit::renderer_status& status) const noexcept;

  [[nodiscard]] granit::result
  upload_scene(gltf::scene scene, gltf_rendering::scene_plan plan,
               std::span<const std::byte> environment_bytes, float sampler_anisotropy,
               gltf_rendering::scene_upload_callback progress = nullptr,
               void* progress_user_data = nullptr);
  [[nodiscard]] granit::result render(frame_packet&& packet, frame_execution_result& output);
  [[nodiscard]] granit::result render_loading_frame(const imgui::frame_canvas_data& data) noexcept;
  [[nodiscard]] granit::result finish_loading() noexcept;
  [[nodiscard]] granit::result initialize_font_atlas(std::span<const std::byte> pixels,
                                                     std::uint32_t width,
                                                     std::uint32_t height) noexcept;
  [[nodiscard]] granit::result
  initialize_pipeline(const granit::render_pipeline_desc& desc) noexcept;
  [[nodiscard]] granit::result begin_pipeline_prepare(granit::texture_format color_format,
                                                      granit::sample_count samples);
  [[nodiscard]] granit::result poll_pipeline_prepare();
  [[nodiscard]] granit::result change_quality(const granit::render_pipeline_desc& desc,
                                              float sampler_anisotropy, bool reupload_scene,
                                              render_quality_change_result& output) noexcept;
  [[nodiscard]] granit::result
  update_material(std::uint32_t material_index,
                  const gltf_rendering::material_factor_update& edit) noexcept;
  [[nodiscard]] granit::result update_debug_display(std::uint32_t mode) noexcept;
  [[nodiscard]] granit::result texture_binding(const gltf::texture_reference& reference, bool srgb,
                                               granit::texture_view_ref& view,
                                               granit::sampler_ref& sampler) const noexcept;
  [[nodiscard]] granit::result recreate_swapchain(const granit::swapchain_desc& desc) noexcept;
  [[nodiscard]] granit::result recreate_surface(granit::surface surface,
                                                const granit::swapchain_desc& desc) noexcept;
  [[nodiscard]] granit::result
  query_resource_stats(granit::renderer_resource_stats& stats) const noexcept;
  /** 销毁 Renderer 前可返回资源统计，用于验证所有子资源已经释放。 */
  [[nodiscard]] granit::result
  shutdown(granit::renderer_resource_stats* final_stats = nullptr) noexcept;

  [[nodiscard]] const granit::renderer_info& renderer_info() const noexcept;
  [[nodiscard]] const granit::renderer_limits& renderer_limits() const noexcept;
  [[nodiscard]] const granit::swapchain_info& swapchain_info() const noexcept;
  [[nodiscard]] granit::texture_view_ref font_view() const noexcept;
  [[nodiscard]] granit::sampler_ref font_sampler() const noexcept;
  [[nodiscard]] granit::renderer_ref renderer() const noexcept;
  [[nodiscard]] const granit::environment_map_info& environment_info() const noexcept;
  /** 仅供浏览器 C ABI 验收钩子使用。 */
  [[nodiscard]] granit_renderer native_renderer() const noexcept;
  /** 仅供浏览器 C ABI 验收钩子使用。 */
  [[nodiscard]] granit_swapchain native_swapchain() const noexcept;
  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] bool presentation_valid() const noexcept;

private:
  struct state;
  std::unique_ptr<state> state_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_RENDERER_H_
