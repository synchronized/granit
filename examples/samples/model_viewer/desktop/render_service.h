// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_DESKTOP_RENDER_SERVICE_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_DESKTOP_RENDER_SERVICE_H_

#include "model_viewer/application_core.h"
#include "model_viewer/frame_executor.h"

#include <granit/granit.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/render_pipeline.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace granit::example::model_viewer::desktop {

struct render_service_state;

using upload_progress_callback = granit::result (*)(unsigned percentage, void* user_data);

struct gpu_upload_desc {
  std::span<const std::byte> environment_bytes;
  float sampler_anisotropy{1.0F};
  upload_progress_callback progress{};
  void* progress_user_data{};
};

struct quality_change_result {
  bool scene_reuploaded{};
};

/** Desktop GPU 命令与帧执行边界；公开方法由主线程调用。 */
class render_service {
public:
  render_service();
  ~render_service();
  render_service(const render_service&) = delete;
  render_service& operator=(const render_service&) = delete;

  [[nodiscard]] granit::result
  initialize(granit::renderer_ref renderer, granit::swapchain& swapchain,
             granit::swapchain_info& swapchain_info, application_core& core,
             std::array<granit::canvas_draw_list, 3>& frame_canvases) noexcept;
  /** 开始不可丢弃上传；desc 中的视图和用户数据必须保持到完成回执。 */
  [[nodiscard]] granit::result begin_gpu_upload(const gpu_upload_desc& desc) noexcept;
  [[nodiscard]] bool try_finish_gpu_upload(granit::result& status) noexcept;
  void cancel_gpu_upload() noexcept;

  [[nodiscard]] granit::result
  initialize_pipeline(const granit::render_pipeline_desc& desc) noexcept;
  [[nodiscard]] granit::result recreate_swapchain(const granit::swapchain_desc& desc) noexcept;
  [[nodiscard]] granit::result change_quality(const granit::render_pipeline_desc& desc,
                                              float sampler_anisotropy, bool reupload_scene,
                                              quality_change_result& output) noexcept;
  [[nodiscard]] granit::result update_material(std::uint32_t material_index,
                                               const material_factor_edit& edit) noexcept;

  [[nodiscard]] bool can_submit_frame() const noexcept;
  void record_skipped_frame_build() noexcept;
  [[nodiscard]] granit::result submit(frame_packet packet, std::uint64_t& sequence) noexcept;
  [[nodiscard]] bool try_take_completion(frame_completion& completion) noexcept;
  [[nodiscard]] render_task_queue_stats query_queue_stats() const noexcept;
  [[nodiscard]] granit::result flush() noexcept;

  [[nodiscard]] granit::result shutdown(granit::renderer& renderer, granit::surface& surface,
                                        granit::texture& font_texture,
                                        granit::texture_view& font_view,
                                        granit::sampler& font_sampler,
                                        granit::canvas_draw_list& loading_canvas) noexcept;
  [[nodiscard]] bool running() const noexcept;

private:
  std::unique_ptr<render_service_state> state_;
};

} // namespace granit::example::model_viewer::desktop

#endif
