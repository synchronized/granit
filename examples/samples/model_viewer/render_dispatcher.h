// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_RENDER_DISPATCHER_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_RENDER_DISPATCHER_H_

#include "model_viewer/viewer_renderer.h"

#include <memory>

namespace granit::example::model_viewer {

/** Desktop/Web 共用的渲染调用门面；执行位置由外部 executor 决定。 */
class render_dispatcher final {
public:
  render_dispatcher();
  ~render_dispatcher();
  render_dispatcher(const render_dispatcher&) = delete;
  render_dispatcher& operator=(const render_dispatcher&) = delete;

  /** executor 不转移所有权，必须保持到 shutdown 或本对象析构之后。 */
  [[nodiscard]] granit::result initialize_renderer(render_execution_policy& executor,
                                                   const granit::renderer_desc& desc) noexcept;
  [[nodiscard]] granit::result complete_renderer_initialization() noexcept;
  [[nodiscard]] granit::result initialize_presentation(granit::window& window,
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
  [[nodiscard]] granit::result
  begin_upload_scene(gltf::scene scene, gltf_rendering::scene_plan plan,
                     std::span<const std::byte> environment_bytes, float sampler_anisotropy,
                     gltf_rendering::scene_upload_callback progress, void* progress_user_data,
                     std::uint64_t& sequence) noexcept;
  /** 仅供已经在 executor 回调内运行的任务使用，避免递归同步排队。 */
  [[nodiscard]] granit::result
  execute_upload_scene(gltf::scene scene, gltf_rendering::scene_plan plan,
                       std::span<const std::byte> environment_bytes, float sampler_anisotropy,
                       gltf_rendering::scene_upload_callback progress = nullptr,
                       void* progress_user_data = nullptr);
  [[nodiscard]] granit::result submit(frame_packet packet, frame_execution_result& output);
  [[nodiscard]] granit::result submit_frame(frame_packet packet, std::uint64_t& sequence) noexcept;
  [[nodiscard]] bool try_take_frame_completion(frame_completion& completion) noexcept;
  [[nodiscard]] bool try_take_control_completion(render_task_completion& completion) noexcept;
  [[nodiscard]] bool can_submit_frame() const noexcept;
  void record_skipped_frame_build() noexcept;
  [[nodiscard]] render_task_queue_stats query_queue_stats() const noexcept;
  [[nodiscard]] granit::result flush() noexcept;
  [[nodiscard]] granit::result render_loading_frame(const imgui::frame_canvas_data& data) noexcept;
  [[nodiscard]] granit::result finish_loading() noexcept;
  [[nodiscard]] granit::result initialize_font_atlas(std::span<const std::byte> pixels,
                                                     std::uint32_t width,
                                                     std::uint32_t height) noexcept;
  [[nodiscard]] granit::result
  initialize_pipeline(const granit::render_pipeline_desc& desc) noexcept;
  [[nodiscard]] granit::result begin_pipeline_prepare(granit::texture_format color_format,
                                                      granit::sample_count samples) noexcept;
  [[nodiscard]] granit::result poll_pipeline_prepare() noexcept;
  [[nodiscard]] granit::result change_quality(const granit::render_pipeline_desc& desc,
                                              float sampler_anisotropy, bool reupload_scene,
                                              render_quality_change_result& output) noexcept;
  [[nodiscard]] granit::result
  update_material(std::uint32_t material_index,
                  const gltf_rendering::material_factor_update& edit) noexcept;
  [[nodiscard]] granit::result update_debug_display(std::uint32_t mode) noexcept;
  [[nodiscard]] granit::result texture_binding(const gltf::texture_reference& reference, bool srgb,
                                               granit::texture_view_ref& view,
                                               granit::sampler_ref& sampler) noexcept;
  [[nodiscard]] granit::result recreate_swapchain(const granit::swapchain_desc& desc) noexcept;
  [[nodiscard]] granit::result recreate_surface(granit::window& window,
                                                const granit::swapchain_desc& desc) noexcept;
  [[nodiscard]] granit::result
  query_resource_stats(granit::renderer_resource_stats& stats) const noexcept;
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
  [[nodiscard]] bool running() const noexcept;

private:
  struct state;
  std::unique_ptr<state> state_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_RENDER_DISPATCHER_H_
