// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include <new>

namespace granit::detail {

webgpu_renderer_state::~webgpu_renderer_state() {
  presentation_.reset();
  commands_.reset();
  pipelines_.reset();
  shaders_.reset();
  if (instance_ != 0) {
    static_cast<void>(loader_.destroy_instance(instance_));
    instance_ = 0;
  }
  loader_.close();
}

std::unique_ptr<backend_command_recorder_resource>
webgpu_renderer_state::allocate_command_recorder_resource() {
  return commands_ ? commands_->allocate_recorder() : nullptr;
}

granit_result
webgpu_renderer_state::create_command_recorder(backend_command_recorder_resource&) noexcept {
  return commands_ ? GRANIT_SUCCESS : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::begin_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->begin(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result
webgpu_renderer_state::end_command_recorder(backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->end(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::reset_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->reset(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::discard_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->reset(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

bool webgpu_renderer_state::command_recorder_is_recording(
    backend_command_recorder_resource&) noexcept {
  return false;
}

granit_result webgpu_renderer_state::draw(backend_command_recorder_resource& recorder,
                                          backend_texture_view_resource* target,
                                          backend_graphics_pipeline_resource* pipeline,
                                          std::uint32_t vertex_count, std::uint32_t instance_count,
                                          std::uint32_t first_vertex,
                                          std::uint32_t first_instance) noexcept {
  if (!commands_ || !presentation_ || !pipelines_ || !target || !pipeline || vertex_count != 3 ||
      instance_count != 1 || first_vertex != 0 || first_instance != 0)
    return GRANIT_ERROR_UNSUPPORTED;
  return commands_->draw(recorder, presentation_->native_view(*target),
                         pipelines_->native_handle(*pipeline));
}

std::unique_ptr<backend_shader_resource> webgpu_renderer_state::allocate_shader_resource() {
  return shaders_ ? shaders_->allocate_shader() : nullptr;
}

granit_result webgpu_renderer_state::create_wgsl_shader(backend_shader_resource& shader,
                                                        granit_shader_stage stage,
                                                        std::string_view source,
                                                        std::string_view entry_point) noexcept {
  return shaders_ ? shaders_->create_shader(shader, stage, source.data(), source.size(),
                                            entry_point.data(), entry_point.size())
                  : GRANIT_ERROR_UNSUPPORTED;
}

std::unique_ptr<backend_pipeline_layout_resource>
webgpu_renderer_state::allocate_pipeline_layout_resource() {
  return pipelines_ ? pipelines_->allocate_pipeline_layout() : nullptr;
}

granit_result webgpu_renderer_state::create_empty_pipeline_layout(
    backend_pipeline_layout_resource& layout) noexcept {
  return pipelines_ ? pipelines_->create_pipeline_layout(layout) : GRANIT_ERROR_UNSUPPORTED;
}

std::unique_ptr<backend_graphics_pipeline_resource>
webgpu_renderer_state::allocate_graphics_pipeline_resource() {
  return pipelines_ ? pipelines_->allocate_graphics_pipeline() : nullptr;
}

granit_result webgpu_renderer_state::validate_graphics_pipeline(
    const granit_graphics_pipeline_desc& desc) const noexcept {
  return pipelines_ ? pipelines_->validate_graphics_pipeline(desc) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::create_graphics_pipeline(
    backend_graphics_pipeline_resource& pipeline, backend_pipeline_layout_resource& layout,
    backend_shader_resource& vertex_shader, backend_shader_resource& fragment_shader,
    granit_texture_format color_format) noexcept {
  if (!pipelines_ || !shaders_)
    return GRANIT_ERROR_UNSUPPORTED;
  return pipelines_->create_graphics_pipeline(
      pipeline, layout, shaders_->native_handle(vertex_shader),
      shaders_->native_handle(fragment_shader), color_format);
}

void* webgpu_renderer_state::allocate(std::uint64_t size, std::uint64_t alignment, void*) noexcept {
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void webgpu_renderer_state::deallocate(void* memory, std::uint64_t, std::uint64_t alignment,
                                       void*) noexcept {
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void webgpu_renderer_state::diagnose(granit_diagnostic_severity severity,
                                     granit_diagnostic_category category, const char* message,
                                     std::uint32_t message_length, void* user_data) noexcept {
  if (user_data == nullptr || (message == nullptr && message_length != 0)) {
    return;
  }
  auto& state = *static_cast<webgpu_renderer_state*>(user_data);
  if (state.diagnostic_callback_ == nullptr) {
    return;
  }
  try {
    state.diagnostic_callback_(severity, category, message, message_length,
                               state.diagnostic_user_data_);
  } catch (...) {
  }
}

granit_result
webgpu_renderer_state::initialize_static(const granit_backend_plugin_api* api,
                                         granit_diagnostic_callback diagnostic_callback,
                                         void* diagnostic_user_data) noexcept {
  if (instance_ != 0 || loader_.is_open()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  diagnostic_callback_ = diagnostic_callback;
  diagnostic_user_data_ = diagnostic_user_data;
  auto result = loader_.open_static(api, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU);
  if (result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, result};
    return result;
  }
  granit_backend_plugin_host_api host{sizeof(host), 0,          diagnose, this,
                                      allocate,     deallocate, nullptr};
  result = loader_.create_instance(&host, &instance_);
  if (result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, result};
    loader_.close();
    return result;
  }
  const auto refresh_result = refresh_state();
  return refresh_result == GRANIT_ERROR_NOT_READY ? GRANIT_SUCCESS : refresh_result;
}

granit_result webgpu_renderer_state::process_backend_events() noexcept {
  if (instance_ == 0) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto result = loader_.process_events(instance_);
  if (result != GRANIT_SUCCESS && result != GRANIT_ERROR_NOT_READY &&
      result != GRANIT_ERROR_DEVICE_LOST) {
    return result;
  }
  const auto refresh_result = refresh_state();
  return refresh_result == GRANIT_ERROR_NOT_READY ? GRANIT_SUCCESS : refresh_result;
}

backend_lifecycle_status webgpu_renderer_state::lifecycle_status() const noexcept {
  return lifecycle_;
}

granit_result webgpu_renderer_state::refresh_state() noexcept {
  granit_backend_plugin_instance_status status{};
  status.struct_size = sizeof(status);
  const auto status_result = loader_.get_instance_status(instance_, &status);
  if (status_result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, status_result};
    return status_result;
  }
  switch (status.state) {
  case GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_INITIALIZING:
    lifecycle_ = {backend_lifecycle_state::initializing, GRANIT_SUCCESS};
    return GRANIT_ERROR_NOT_READY;
  case GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_FAILED:
    lifecycle_ = {backend_lifecycle_state::failed, status.failure_result};
    return status.failure_result;
  case GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_DEVICE_LOST:
    lifecycle_ = {backend_lifecycle_state::device_lost, status.failure_result};
    return status.failure_result;
  case GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_READY:
    break;
  default:
    lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_INTERNAL};
    return GRANIT_ERROR_INTERNAL;
  }

  if (presentation_ == nullptr || shaders_ == nullptr || pipelines_ == nullptr ||
      commands_ == nullptr) {
    granit_backend_plugin_capabilities capabilities{};
    capabilities.struct_size = sizeof(capabilities);
    const auto capabilities_result = loader_.get_capabilities(instance_, &capabilities);
    if (capabilities_result != GRANIT_SUCCESS) {
      lifecycle_ = {backend_lifecycle_state::failed, capabilities_result};
      return capabilities_result;
    }
    capabilities_ = {
        capabilities.uniform_buffer_offset_alignment,
        capabilities.storage_buffer_offset_alignment,
        capabilities.max_uniform_buffer_binding_size,
        capabilities.max_storage_buffer_binding_size,
    };
    try {
      auto presentation = std::make_unique<webgpu_presentation_adapter>(loader_, instance_);
      auto shaders = std::make_unique<webgpu_shader_adapter>(loader_, instance_);
      auto pipelines = std::make_unique<webgpu_pipeline_adapter>(loader_, instance_);
      auto commands = std::make_unique<webgpu_command_adapter>(loader_, instance_);
      presentation_ = std::move(presentation);
      shaders_ = std::move(shaders);
      pipelines_ = std::move(pipelines);
      commands_ = std::move(commands);
    } catch (const std::bad_alloc&) {
      lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_OUT_OF_MEMORY};
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_INTERNAL};
      return GRANIT_ERROR_INTERNAL;
    }
  }
  lifecycle_ = {backend_lifecycle_state::ready, GRANIT_SUCCESS};
  return GRANIT_SUCCESS;
}

std::unique_ptr<backend_surface_resource> webgpu_renderer_state::allocate_surface_resource() {
  return presentation_ != nullptr ? presentation_->allocate_surface() : nullptr;
}

std::unique_ptr<backend_swapchain_resource> webgpu_renderer_state::allocate_swapchain_resource() {
  return presentation_ != nullptr ? presentation_->allocate_swapchain() : nullptr;
}

granit_result webgpu_renderer_state::create_win32_surface(void*, void*,
                                                          backend_surface_resource&) noexcept {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::create_xcb_surface(void*, std::uint32_t,
                                                        backend_surface_resource&) noexcept {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::create_wayland_surface(void*, void*,
                                                            backend_surface_resource&) noexcept {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result
webgpu_renderer_state::create_canvas_surface(std::string_view selector,
                                             backend_surface_resource& surface) noexcept {
  if (presentation_ == nullptr)
    return GRANIT_ERROR_NOT_READY;
  return presentation_->create_canvas_surface(surface, selector.data(),
                                              static_cast<std::uint32_t>(selector.size()));
}

granit_result webgpu_renderer_state::create_swapchain(backend_surface_resource& surface,
                                                      const backend_swapchain_desc& desc,
                                                      backend_swapchain_resource& swapchain) {
  return presentation_ != nullptr ? presentation_->create_swapchain(surface, desc, swapchain)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::recreate_swapchain(backend_surface_resource&,
                                                        const backend_swapchain_desc& desc,
                                                        backend_swapchain_resource& swapchain) {
  return presentation_ != nullptr ? presentation_->recreate_swapchain(swapchain, desc)
                                  : GRANIT_ERROR_NOT_READY;
}

backend_swapchain_info
webgpu_renderer_state::get_swapchain_info(backend_swapchain_resource& swapchain) noexcept {
  backend_swapchain_info info{};
  if (presentation_ != nullptr)
    static_cast<void>(presentation_->get_swapchain_info(swapchain, info));
  return info;
}

granit_result webgpu_renderer_state::get_swapchain_backbuffers(
    backend_swapchain_resource&, std::vector<backend_swapchain_backbuffer>& backbuffers) {
  // WebGPU 的当前纹理由 Acquire 动态提供，不存在可预先枚举的固定后备缓冲。
  backbuffers.clear();
  return presentation_ != nullptr ? GRANIT_SUCCESS : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_renderer_state::prepare_swapchain_backbuffer(backend_swapchain_backbuffer& backbuffer) {
  return backbuffer.texture && backbuffer.view ? GRANIT_SUCCESS : GRANIT_ERROR_INTERNAL;
}

granit_result
webgpu_renderer_state::acquire_swapchain_frame(backend_swapchain_resource& swapchain,
                                               backend_acquired_swapchain_frame& frame) {
  return presentation_ != nullptr ? presentation_->acquire_swapchain(swapchain, frame)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::present_swapchain_frame(backend_swapchain_resource& swapchain,
                                                             std::uint32_t, std::size_t,
                                                             bool& needs_recreate) {
  return presentation_ != nullptr ? presentation_->present_swapchain(swapchain, needs_recreate)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::cancel_swapchain_frame(backend_swapchain_resource& swapchain,
                                                            std::uint32_t, std::size_t,
                                                            bool& needs_recreate) {
  return presentation_ != nullptr ? presentation_->cancel_swapchain(swapchain, needs_recreate)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::wait_for_present_idle() noexcept {
  return lifecycle_.state == backend_lifecycle_state::device_lost ? GRANIT_ERROR_DEVICE_LOST
                                                                  : GRANIT_SUCCESS;
}

std::size_t webgpu_renderer_state::collect_present_retired() noexcept { return 0; }

std::size_t webgpu_renderer_state::frame_slot_count() const noexcept {
  // 浏览器交换链按 Acquire 返回动态纹理，当前只允许一个在途呈现帧。
  return 1;
}

granit_result
webgpu_renderer_state::submit_command_recorder(backend_command_recorder_resource& recorder,
                                               submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (commands_ == nullptr)
    return GRANIT_ERROR_NOT_READY;
  const auto result = commands_->submit(recorder);
  if (result == GRANIT_SUCCESS)
    submitted_serial = next_submission_serial_++;
  return result;
}

granit_result webgpu_renderer_state::submit_command_recorders(
    std::span<backend_command_recorder_resource* const> recorders,
    submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (recorders.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (auto* recorder : recorders) {
    if (recorder == nullptr)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto result = commands_->submit(*recorder);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  submitted_serial = next_submission_serial_++;
  return GRANIT_SUCCESS;
}

granit_result
webgpu_renderer_state::wait_command_recorder(backend_command_recorder_resource&) noexcept {
  return GRANIT_SUCCESS;
}

granit_result webgpu_renderer_state::wait_for_all_submissions() noexcept {
  return lifecycle_.state == backend_lifecycle_state::device_lost ? GRANIT_ERROR_DEVICE_LOST
                                                                  : GRANIT_SUCCESS;
}

granit_result
webgpu_renderer_state::submit_swapchain_frame(backend_command_recorder_resource& recorder,
                                              backend_swapchain_resource&, std::uint32_t,
                                              std::size_t, submission_serial& submitted_serial) {
  return submit_command_recorder(recorder, submitted_serial);
}

} // namespace granit::detail
