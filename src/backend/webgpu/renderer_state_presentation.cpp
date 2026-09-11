// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <vector>

namespace granit::detail {

std::unique_ptr<backend_surface_resource> webgpu_renderer_state::allocate_surface_resource() {
  return capabilities_initialized_ ? presentation_allocate_surface() : nullptr;
}

std::unique_ptr<backend_swapchain_resource> webgpu_renderer_state::allocate_swapchain_resource() {
  return capabilities_initialized_ ? presentation_allocate_swapchain() : nullptr;
}

granit_result webgpu_renderer_state::create_surface(const granit_surface_desc& desc,
                                                    backend_surface_resource& surface) noexcept {
  std::uint32_t device_type{};
  switch (desc.surface_type) {
  case GRANIT_SURFACE_TYPE_WIN32_BIT:
    device_type = GRANIT_WEBGPU_SURFACE_TYPE_WIN32_BIT;
    break;
  case GRANIT_SURFACE_TYPE_XCB_BIT:
    device_type = GRANIT_WEBGPU_SURFACE_TYPE_XCB_BIT;
    break;
  case GRANIT_SURFACE_TYPE_WAYLAND_BIT:
    device_type = GRANIT_WEBGPU_SURFACE_TYPE_WAYLAND_BIT;
    break;
  case GRANIT_SURFACE_TYPE_CANVAS_BIT:
    device_type = GRANIT_WEBGPU_SURFACE_TYPE_CANVAS_BIT;
    break;
  default:
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if ((surface_types_ & desc.surface_type) == 0 || (device_surface_types_ & device_type) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  if (!capabilities_initialized_)
    return GRANIT_ERROR_NOT_READY;
  return presentation_create_surface(desc, surface);
}

granit_result webgpu_renderer_state::create_swapchain(backend_surface_resource& surface,
                                                      const backend_swapchain_desc& desc,
                                                      backend_swapchain_resource& swapchain) {
  return capabilities_initialized_ ? presentation_create_swapchain(surface, desc, swapchain)
                                   : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::recreate_swapchain(backend_surface_resource&,
                                                        const backend_swapchain_desc& desc,
                                                        backend_swapchain_resource& swapchain) {
  return capabilities_initialized_ ? presentation_recreate_swapchain(swapchain, desc)
                                   : GRANIT_ERROR_NOT_READY;
}

backend_swapchain_info
webgpu_renderer_state::get_swapchain_info(backend_swapchain_resource& swapchain) noexcept {
  backend_swapchain_info info{};
  if (capabilities_initialized_)
    static_cast<void>(presentation_get_swapchain_info(swapchain, info));
  return info;
}

granit_result webgpu_renderer_state::get_swapchain_backbuffers(
    backend_swapchain_resource&, std::vector<backend_swapchain_backbuffer>& backbuffers) {
  // WebGPU 的当前纹理由 Acquire 动态提供，不存在可预先枚举的固定后备缓冲。
  backbuffers.clear();
  return capabilities_initialized_ ? GRANIT_SUCCESS : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_renderer_state::prepare_swapchain_backbuffer(backend_swapchain_backbuffer& backbuffer) {
  return backbuffer.texture && backbuffer.view ? GRANIT_SUCCESS : GRANIT_ERROR_INTERNAL;
}

granit_result
webgpu_renderer_state::acquire_swapchain_frame(backend_swapchain_resource& swapchain,
                                               backend_acquired_swapchain_frame& frame) {
  return capabilities_initialized_ ? presentation_acquire_swapchain(swapchain, frame)
                                   : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::present_swapchain_frame(backend_swapchain_resource& swapchain,
                                                             std::uint32_t, std::size_t,
                                                             bool& needs_recreate) {
  return capabilities_initialized_ ? presentation_present_swapchain(swapchain, needs_recreate)
                                   : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::cancel_swapchain_frame(backend_swapchain_resource& swapchain,
                                                            std::uint32_t, std::size_t,
                                                            bool& needs_recreate) {
  return capabilities_initialized_ ? presentation_cancel_swapchain(swapchain, needs_recreate)
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
webgpu_renderer_state::submit_swapchain_frame(backend_command_recorder_resource& recorder,
                                              backend_swapchain_resource&, std::uint32_t,
                                              std::size_t, submission_serial& submitted_serial) {
  return submit_command_recorder(recorder, submitted_serial);
}

} // namespace granit::detail
