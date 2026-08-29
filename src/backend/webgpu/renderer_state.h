// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_RENDERER_STATE_H_
#define GRANIT_BACKEND_WEBGPU_RENDERER_STATE_H_

#include <memory>

#include <granit/core/diagnostic.h>

#include "backend/capabilities.h"
#include "backend/command.h"
#include "backend/lifecycle.h"
#include "backend/pipeline.h"
#include "backend/plugin_loader.h"
#include "backend/queue.h"
#include "backend/renderer.h"
#include "backend/rendering.h"
#include "backend/shader.h"
#include "backend/webgpu/command_adapter.h"
#include "backend/webgpu/pipeline_adapter.h"
#include "backend/webgpu/presentation_adapter.h"
#include "backend/webgpu/shader_adapter.h"

namespace granit::detail {

/** 集中管理 WebGPU Provider、异步生命周期、能力快照和呈现适配器。 */
class webgpu_renderer_state final : public backend_renderer,
                                    public backend_presentation_renderer,
                                    public backend_queue,
                                    public backend_command_renderer,
                                    public backend_graphics_command_renderer,
                                    public backend_shader_renderer,
                                    public backend_pipeline_layout_renderer,
                                    public backend_pipeline_renderer {
public:
  webgpu_renderer_state() = default;
  ~webgpu_renderer_state();

  webgpu_renderer_state(const webgpu_renderer_state&) = delete;
  webgpu_renderer_state& operator=(const webgpu_renderer_state&) = delete;

  [[nodiscard]] granit_result initialize_static(const granit_backend_plugin_api* api,
                                                std::uint32_t surface_types,
                                                granit_diagnostic_callback diagnostic_callback,
                                                void* diagnostic_user_data) noexcept;
  [[nodiscard]] granit_result initialize_dynamic(std::string_view library_path,
                                                 std::uint32_t surface_types,
                                                 granit_diagnostic_callback diagnostic_callback,
                                                 void* diagnostic_user_data) noexcept;
  [[nodiscard]] granit_result process_backend_events() noexcept override;
  [[nodiscard]] granit_renderer_backend backend() const noexcept override {
    return GRANIT_RENDERER_BACKEND_WEBGPU;
  }
  [[nodiscard]] std::string_view adapter_name() const noexcept override { return {}; }
  [[nodiscard]] std::uint32_t adapter_vendor_id() const noexcept override { return 0; }
  [[nodiscard]] std::uint32_t adapter_device_id() const noexcept override { return 0; }

  [[nodiscard]] backend_lifecycle_status lifecycle_status() const noexcept override;
  [[nodiscard]] const backend_capabilities& capabilities() const noexcept override {
    return capabilities_;
  }
  [[nodiscard]] std::uint32_t domain() const noexcept override { return domain_; }
  void set_domain(std::uint32_t domain) noexcept override { domain_ = domain; }
  [[nodiscard]] webgpu_presentation_adapter* presentation() noexcept { return presentation_.get(); }

  [[nodiscard]] std::unique_ptr<backend_command_recorder_resource>
  allocate_command_recorder_resource() override;
  [[nodiscard]] granit_result
  create_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  begin_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  end_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  reset_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result
  discard_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] bool
  command_recorder_is_recording(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result draw(backend_command_recorder_resource& recorder,
                                   backend_texture_view_resource* target,
                                   backend_graphics_pipeline_resource* pipeline,
                                   std::uint32_t vertex_count, std::uint32_t instance_count,
                                   std::uint32_t first_vertex,
                                   std::uint32_t first_instance) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_shader_resource> allocate_shader_resource() override;
  [[nodiscard]] granit_result create_wgsl_shader(backend_shader_resource& shader,
                                                 granit_shader_stage stage, std::string_view source,
                                                 std::string_view entry_point) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_pipeline_layout_resource>
  allocate_pipeline_layout_resource() override;
  [[nodiscard]] granit_result
  create_pipeline_layout(std::span<backend_bind_group_layout_resource* const> bind_group_layouts,
                         backend_pipeline_layout_resource& layout) noexcept override;
  [[nodiscard]] std::unique_ptr<backend_graphics_pipeline_resource>
  allocate_graphics_pipeline_resource() override;
  [[nodiscard]] granit_result
  validate_graphics_pipeline(const granit_graphics_pipeline_desc& desc) const noexcept override;
  [[nodiscard]] granit_result
  create_graphics_pipeline(const backend_graphics_pipeline_create_info& info,
                           backend_graphics_pipeline_resource& pipeline) noexcept override;

  [[nodiscard]] std::unique_ptr<backend_surface_resource> allocate_surface_resource() override;
  [[nodiscard]] std::unique_ptr<backend_swapchain_resource> allocate_swapchain_resource() override;
  [[nodiscard]] granit_result create_win32_surface(void*, void*,
                                                   backend_surface_resource&) noexcept override;
  [[nodiscard]] granit_result create_xcb_surface(void*, std::uint32_t,
                                                 backend_surface_resource&) noexcept override;
  [[nodiscard]] granit_result create_wayland_surface(void*, void*,
                                                     backend_surface_resource&) noexcept override;
  [[nodiscard]] granit_result
  create_canvas_surface(std::string_view selector,
                        backend_surface_resource& surface) noexcept override;
  [[nodiscard]] granit_result create_swapchain(backend_surface_resource& surface,
                                               const backend_swapchain_desc& desc,
                                               backend_swapchain_resource& swapchain) override;
  [[nodiscard]] granit_result recreate_swapchain(backend_surface_resource& surface,
                                                 const backend_swapchain_desc& desc,
                                                 backend_swapchain_resource& swapchain) override;
  [[nodiscard]] backend_swapchain_info
  get_swapchain_info(backend_swapchain_resource& swapchain) noexcept override;
  [[nodiscard]] granit_result
  get_swapchain_backbuffers(backend_swapchain_resource& swapchain,
                            std::vector<backend_swapchain_backbuffer>& backbuffers) override;
  [[nodiscard]] granit_result
  prepare_swapchain_backbuffer(backend_swapchain_backbuffer& backbuffer) override;
  [[nodiscard]] granit_result
  acquire_swapchain_frame(backend_swapchain_resource& swapchain,
                          backend_acquired_swapchain_frame& frame) override;
  [[nodiscard]] granit_result present_swapchain_frame(backend_swapchain_resource& swapchain,
                                                      std::uint32_t image_index,
                                                      std::size_t slot_index,
                                                      bool& needs_recreate) override;
  [[nodiscard]] granit_result cancel_swapchain_frame(backend_swapchain_resource& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     bool& needs_recreate) override;
  [[nodiscard]] granit_result wait_for_present_idle() noexcept override;
  std::size_t collect_present_retired() noexcept override;
  [[nodiscard]] std::size_t frame_slot_count() const noexcept override;
  [[nodiscard]] granit_result submit_command_recorder(backend_command_recorder_resource& recorder,
                                                      submission_serial& submitted_serial) override;
  [[nodiscard]] granit_result
  submit_command_recorders(std::span<backend_command_recorder_resource* const> recorders,
                           submission_serial& submitted_serial) override;
  [[nodiscard]] granit_result
  wait_command_recorder(backend_command_recorder_resource& recorder) noexcept override;
  [[nodiscard]] granit_result wait_for_all_submissions() noexcept override;
  [[nodiscard]] granit_result submit_swapchain_frame(backend_command_recorder_resource& recorder,
                                                     backend_swapchain_resource& swapchain,
                                                     std::uint32_t image_index,
                                                     std::size_t slot_index,
                                                     submission_serial& submitted_serial) override;

private:
  static void* allocate(std::uint64_t size, std::uint64_t alignment, void*) noexcept;
  static void deallocate(void* memory, std::uint64_t size, std::uint64_t alignment, void*) noexcept;
  static void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category category,
                       const char* message, std::uint32_t message_length, void* user_data) noexcept;
  [[nodiscard]] granit_result refresh_state() noexcept;
  [[nodiscard]] granit_result finish_initialization() noexcept;

  backend_plugin_loader loader_;
  granit_backend_plugin_instance instance_{};
  granit_diagnostic_callback diagnostic_callback_{};
  void* diagnostic_user_data_{};
  backend_lifecycle_status lifecycle_{};
  backend_capabilities capabilities_{};
  std::uint32_t surface_types_{};
  std::uint32_t provider_surface_types_{};
  std::uint32_t domain_{};
  submission_serial next_submission_serial_{1};
  std::unique_ptr<webgpu_presentation_adapter> presentation_;
  std::unique_ptr<webgpu_shader_adapter> shaders_;
  std::unique_ptr<webgpu_pipeline_adapter> pipelines_;
  std::unique_ptr<webgpu_command_adapter> commands_;
};

} // namespace granit::detail

#endif
