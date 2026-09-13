// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device.h"
#include "backend/webgpu/device_state.h"
#include "backend/webgpu/device_utils.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>

#include <webgpu/webgpu.h>

namespace {

using granit::detail::webgpu_device_state;
using namespace granit::detail::webgpu_native;

WGPUStatus present_surface(WGPUSurface surface) noexcept {
#if defined(__EMSCRIPTEN__)
  // 浏览器在 requestAnimationFrame 边界隐式呈现，Emscripten 禁止显式调用 Present。
  static_cast<void>(surface);
  return WGPUStatus_Success;
#else
  return wgpuSurfacePresent(surface);
#endif
}

template <typename NativeDesc>
granit_result create_native_surface(webgpu_instance_handle instance, NativeDesc source,
                                    webgpu_surface* surface) noexcept {
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  WGPUSurfaceDescriptor native_desc{};
  native_desc.nextInChain = &source.chain;
  const auto native_surface = wgpuInstanceCreateSurface(found->second->instance, &native_desc);
  if (native_surface == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  const auto handle = next_handle<webgpu_surface>(next_surface);
  try {
    found->second->surfaces.emplace(handle,
                                    webgpu_device_state::surface_record{native_surface, {}});
  } catch (const std::bad_alloc&) {
    wgpuSurfaceRelease(native_surface);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuSurfaceRelease(native_surface);
    return GRANIT_ERROR_INTERNAL;
  }
  *surface = handle;
  return GRANIT_SUCCESS;
}

granit_result create_win32_surface(webgpu_instance_handle instance, void* native_instance,
                                   void* native_window, webgpu_surface* surface) noexcept {
  if (instance == 0 || native_instance == nullptr || native_window == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if (defined(_WIN32) && !defined(__EMSCRIPTEN__)) || defined(GRANIT_WEBGPU_NATIVE_SURFACE_TEST)
  WGPUSurfaceSourceWindowsHWND source{};
  source.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
  source.hinstance = native_instance;
  source.hwnd = native_window;
  return create_native_surface(instance, source, surface);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result create_xcb_surface(webgpu_instance_handle instance, void* connection,
                                 std::uint32_t window, webgpu_surface* surface) noexcept {
  if (instance == 0 || connection == nullptr || window == 0 || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if (defined(__linux__) && !defined(__EMSCRIPTEN__)) || defined(GRANIT_WEBGPU_NATIVE_SURFACE_TEST)
  WGPUSurfaceSourceXCBWindow source{};
  source.chain.sType = WGPUSType_SurfaceSourceXCBWindow;
  source.connection = connection;
  source.window = window;
  return create_native_surface(instance, source, surface);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result create_wayland_surface(webgpu_instance_handle instance, void* display,
                                     void* native_surface, webgpu_surface* surface) noexcept {
  if (instance == 0 || display == nullptr || native_surface == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if (defined(__linux__) && !defined(__EMSCRIPTEN__)) || defined(GRANIT_WEBGPU_NATIVE_SURFACE_TEST)
  WGPUSurfaceSourceWaylandSurface source{};
  source.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
  source.display = display;
  source.surface = native_surface;
  return create_native_surface(instance, source, surface);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result create_canvas_surface(webgpu_instance_handle instance, const char* selector,
                                    std::uint32_t selector_length,
                                    webgpu_surface* surface) noexcept {
  if (instance == 0 || selector == nullptr || selector_length == 0 || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
#if defined(__EMSCRIPTEN__) || defined(GRANIT_WEBGPU_CANVAS_SURFACE_TEST)
  try {
    std::string selector_copy{selector, selector_length};
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc{};
    canvas_desc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvas_desc.selector = {selector_copy.data(), selector_copy.size()};
    WGPUSurfaceDescriptor native_desc{};
    native_desc.nextInChain = &canvas_desc.chain;
    const auto native_surface = wgpuInstanceCreateSurface(found->second->instance, &native_desc);
    if (native_surface == nullptr)
      return GRANIT_ERROR_INITIALIZATION_FAILED;
    const auto handle = next_handle<webgpu_surface>(next_surface);
    try {
      found->second->surfaces.emplace(
          handle, webgpu_device_state::surface_record{native_surface, std::move(selector_copy)});
    } catch (...) {
      wgpuSurfaceRelease(native_surface);
      throw;
    }
    *surface = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result create_surface(webgpu_instance_handle instance, const granit_surface_desc* desc,
                             webgpu_surface* surface) noexcept {
  if (surface != nullptr)
    *surface = 0;
  if (instance == 0 || desc == nullptr || surface == nullptr ||
      desc->struct_size < GRANIT_SURFACE_DESC_VERSION_1_SIZE || desc->flags != 0 ||
      desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  switch (desc->surface_type) {
  case GRANIT_SURFACE_TYPE_WIN32_BIT:
    return create_win32_surface(instance, desc->source.win32.instance, desc->source.win32.window,
                                surface);
  case GRANIT_SURFACE_TYPE_XCB_BIT:
    if (desc->source.xcb.reserved != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    return create_xcb_surface(instance, desc->source.xcb.connection, desc->source.xcb.window,
                              surface);
  case GRANIT_SURFACE_TYPE_WAYLAND_BIT:
    return create_wayland_surface(instance, desc->source.wayland.display,
                                  desc->source.wayland.surface, surface);
  case GRANIT_SURFACE_TYPE_CANVAS_BIT:
    if (desc->source.canvas.reserved != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    return create_canvas_surface(instance, desc->source.canvas.selector,
                                 desc->source.canvas.selector_length, surface);
  default:
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
}

granit_result destroy_surface(webgpu_instance_handle instance, webgpu_surface surface) noexcept {
  if (instance == 0 || surface == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto surface_found = found->second->surfaces.find(surface);
  if (surface_found == found->second->surfaces.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (std::any_of(found->second->swapchains.begin(), found->second->swapchains.end(),
                  [surface](const auto& entry) { return entry.second.surface == surface; }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuSurfaceRelease(static_cast<WGPUSurface>(surface_found->second.surface));
  found->second->surfaces.erase(surface_found);
  return GRANIT_SUCCESS;
}

granit_result configure_swapchain(webgpu_device_state& state, WGPUSurface surface,
                                  const webgpu_swapchain_desc& desc,
                                  webgpu_swapchain_info& info) noexcept {
  WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
  if (wgpuSurfaceGetCapabilities(surface, state.adapter, &capabilities) != WGPUStatus_Success)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto release_capabilities = [&capabilities] {
    wgpuSurfaceCapabilitiesFreeMembers(capabilities);
  };
  WGPUTextureFormat format{};
  for (std::size_t index = 0; index < capabilities.formatCount; ++index) {
    if (capabilities.formats[index] == WGPUTextureFormat_RGBA8Unorm) {
      format = capabilities.formats[index];
      break;
    }
  }
  if (format == WGPUTextureFormat_Undefined) {
    for (std::size_t index = 0; index < capabilities.formatCount; ++index) {
      if (capabilities.formats[index] == WGPUTextureFormat_BGRA8Unorm) {
        format = capabilities.formats[index];
        break;
      }
    }
  }
  if (format == 0) {
    release_capabilities();
    return GRANIT_ERROR_UNSUPPORTED;
  }
  const WGPUPresentMode requested_mode =
      desc.present_mode == GRANIT_WEBGPU_PRESENT_MODE_MAILBOX     ? WGPUPresentMode_Mailbox
      : desc.present_mode == GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE ? WGPUPresentMode_Immediate
                                                                  : WGPUPresentMode_Fifo;
  WGPUPresentMode selected_mode = WGPUPresentMode_Fifo;
  for (std::size_t index = 0; index < capabilities.presentModeCount; ++index) {
    if (capabilities.presentModes[index] == requested_mode) {
      selected_mode = requested_mode;
      break;
    }
  }
  WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
  configuration.device = state.device;
  configuration.format = format;
  configuration.usage = WGPUTextureUsage_RenderAttachment;
  configuration.width = desc.width;
  configuration.height = desc.height;
  configuration.presentMode = selected_mode;
  configuration.alphaMode = WGPUCompositeAlphaMode_Auto;
  wgpuSurfaceConfigure(surface, &configuration);
  release_capabilities();
  info = {sizeof(webgpu_swapchain_info),
          desc.width,
          desc.height,
          1,
          selected_mode == WGPUPresentMode_Mailbox     ? GRANIT_WEBGPU_PRESENT_MODE_MAILBOX
          : selected_mode == WGPUPresentMode_Immediate ? GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE
                                                       : GRANIT_WEBGPU_PRESENT_MODE_FIFO,
          format == WGPUTextureFormat_BGRA8Unorm ? GRANIT_WEBGPU_TEXTURE_FORMAT_BGRA8_UNORM
                                                 : GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_UNORM};
  return GRANIT_SUCCESS;
}

granit_result create_swapchain(webgpu_instance_handle instance, webgpu_surface surface,
                               const webgpu_swapchain_desc* desc,
                               webgpu_swapchain* swapchain) noexcept {
  if (swapchain != nullptr)
    *swapchain = 0;
  if (instance == 0 || surface == 0 || desc == nullptr || swapchain == nullptr ||
      desc->struct_size < sizeof(webgpu_swapchain_desc) || desc->width == 0 || desc->height == 0 ||
      desc->present_mode > GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end() ||
      found->second->surfaces.find(surface) == found->second->surfaces.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  if (std::any_of(found->second->swapchains.begin(), found->second->swapchains.end(),
                  [surface](const auto& entry) { return entry.second.surface == surface; }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto native_surface =
      static_cast<WGPUSurface>(found->second->surfaces.find(surface)->second.surface);
  webgpu_swapchain_info info{};
  if (const auto result = configure_swapchain(*found->second, native_surface, *desc, info);
      result != GRANIT_SUCCESS)
    return result;
  const auto handle = next_handle<webgpu_swapchain>(next_swapchain);
  try {
    found->second->swapchains.emplace(
        handle, webgpu_device_state::swapchain_record{surface, native_surface, info, 0, 0});
  } catch (const std::bad_alloc&) {
    wgpuSurfaceUnconfigure(native_surface);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuSurfaceUnconfigure(native_surface);
    return GRANIT_ERROR_INTERNAL;
  }
  *swapchain = handle;
  return GRANIT_SUCCESS;
}

granit_result recreate_swapchain(webgpu_instance_handle instance, webgpu_swapchain swapchain,
                                 const webgpu_swapchain_desc* desc) noexcept {
  if (instance == 0 || swapchain == 0 || desc == nullptr ||
      desc->struct_size < sizeof(webgpu_swapchain_desc) || desc->width == 0 || desc->height == 0 ||
      desc->present_mode > GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (swapchain_found->second.acquired_texture != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  webgpu_swapchain_info info{};
  const auto result = configure_swapchain(
      *found->second, static_cast<WGPUSurface>(swapchain_found->second.native_surface), *desc,
      info);
  if (result == GRANIT_SUCCESS)
    swapchain_found->second.info = info;
  return result;
}

granit_result get_swapchain_info(webgpu_instance_handle instance, webgpu_swapchain swapchain,
                                 webgpu_swapchain_info* info) noexcept {
  if (instance == 0 || swapchain == 0 || info == nullptr ||
      info->struct_size < sizeof(webgpu_swapchain_info))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  *info = swapchain_found->second.info;
  return GRANIT_SUCCESS;
}

granit_result acquire_swapchain(webgpu_instance_handle instance, webgpu_swapchain swapchain,
                                webgpu_acquired_frame* frame) noexcept {
  if (instance == 0 || swapchain == 0 || frame == nullptr ||
      frame->struct_size < sizeof(webgpu_acquired_frame) || frame->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (swapchain_found->second.acquired_texture != 0)
    return GRANIT_ERROR_NOT_READY;
  WGPUSurfaceTexture acquired{};
  wgpuSurfaceGetCurrentTexture(static_cast<WGPUSurface>(swapchain_found->second.native_surface),
                               &acquired);
  const auto suboptimal = acquired.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;
  if (acquired.texture == nullptr)
    return acquired.status == WGPUSurfaceGetCurrentTextureStatus_Timeout ? GRANIT_ERROR_NOT_READY
           : acquired.status == WGPUSurfaceGetCurrentTextureStatus_Outdated
               ? GRANIT_ERROR_OUT_OF_DATE
           : acquired.status == WGPUSurfaceGetCurrentTextureStatus_Lost ? GRANIT_ERROR_SURFACE_LOST
                                                                        : GRANIT_ERROR_INTERNAL;
  const auto native_view = wgpuTextureCreateView(acquired.texture, nullptr);
  if (native_view == nullptr) {
    static_cast<void>(
        present_surface(static_cast<WGPUSurface>(swapchain_found->second.native_surface)));
    wgpuTextureRelease(acquired.texture);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  const auto texture = next_handle<webgpu_texture>(next_texture);
  const auto view = next_handle<webgpu_texture_view>(next_texture_view);
  try {
    found->second->textures.emplace(
        texture, webgpu_device_state::texture_record{
                     acquired.texture, swapchain_found->second.info.width,
                     swapchain_found->second.info.height, swapchain_found->second.info.format, 1, 1,
                     1, GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT, true});
    found->second->texture_views.emplace(
        view, webgpu_device_state::texture_view_record{native_view, texture, true});
  } catch (...) {
    found->second->texture_views.erase(view);
    found->second->textures.erase(texture);
    wgpuTextureViewRelease(native_view);
    static_cast<void>(
        present_surface(static_cast<WGPUSurface>(swapchain_found->second.native_surface)));
    wgpuTextureRelease(acquired.texture);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  swapchain_found->second.acquired_texture = texture;
  swapchain_found->second.acquired_view = view;
  *frame = {sizeof(webgpu_acquired_frame), 0, suboptimal ? 1U : 0U, 0, texture, view};
  return GRANIT_SUCCESS;
}

granit_result finish_swapchain_frame(webgpu_device_state& state,
                                     webgpu_device_state::swapchain_record& swapchain,
                                     std::uint32_t& needs_recreate) noexcept {
  needs_recreate = 0;
  if (swapchain.acquired_texture == 0 || swapchain.acquired_view == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto view = state.texture_views.find(swapchain.acquired_view);
  const auto texture = state.textures.find(swapchain.acquired_texture);
  if (view == state.texture_views.end() || texture == state.textures.end())
    return GRANIT_ERROR_INTERNAL;
  const auto present_result = present_surface(static_cast<WGPUSurface>(swapchain.native_surface));
  wgpuTextureViewRelease(view->second.view);
  wgpuTextureRelease(texture->second.texture);
  state.texture_views.erase(view);
  state.textures.erase(texture);
  swapchain.acquired_view = 0;
  swapchain.acquired_texture = 0;
  return present_result == WGPUStatus_Success ? GRANIT_SUCCESS : GRANIT_ERROR_OUT_OF_DATE;
}

granit_result present_swapchain(webgpu_instance_handle instance, webgpu_swapchain swapchain,
                                std::uint32_t* needs_recreate) noexcept {
  if (instance == 0 || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  return finish_swapchain_frame(*found->second, swapchain_found->second, *needs_recreate);
}

granit_result cancel_swapchain(webgpu_instance_handle instance, webgpu_swapchain swapchain,
                               std::uint32_t* needs_recreate) noexcept {
  if (instance == 0 || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  return finish_swapchain_frame(*found->second, swapchain_found->second, *needs_recreate);
}

granit_result destroy_swapchain(webgpu_instance_handle instance,
                                webgpu_swapchain swapchain) noexcept {
  if (instance == 0 || swapchain == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (swapchain_found->second.acquired_texture != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuSurfaceUnconfigure(static_cast<WGPUSurface>(swapchain_found->second.native_surface));
  found->second->swapchains.erase(swapchain_found);
  return GRANIT_SUCCESS;
}

} // namespace

namespace granit::detail {

granit_result webgpu_device::create_surface(const granit_surface_desc* desc,
                                            webgpu_surface* surface) noexcept {
  if (!open_ || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_surface(instance_, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_surface(webgpu_surface surface) noexcept {
  if (!open_ || surface == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_surface(instance_, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_swapchain(webgpu_surface surface,
                                              const webgpu_swapchain_desc* desc,
                                              webgpu_swapchain* swapchain) noexcept {
  if (!open_ || surface == 0 || desc == nullptr || swapchain == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_swapchain(instance_, surface, desc, swapchain);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recreate_swapchain(webgpu_swapchain swapchain,
                                                const webgpu_swapchain_desc* desc) noexcept {
  if (!open_ || swapchain == 0 || desc == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recreate_swapchain(instance_, swapchain, desc);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::get_swapchain_info(webgpu_swapchain swapchain,
                                                webgpu_swapchain_info* info) noexcept {
  if (!open_ || swapchain == 0 || info == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::get_swapchain_info(instance_, swapchain, info);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::acquire_swapchain(webgpu_swapchain swapchain,
                                               webgpu_acquired_frame* frame) noexcept {
  if (!open_ || swapchain == 0 || frame == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::acquire_swapchain(instance_, swapchain, frame);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::present_swapchain(webgpu_swapchain swapchain,
                                               std::uint32_t* needs_recreate) noexcept {
  if (!open_ || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::present_swapchain(instance_, swapchain, needs_recreate);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::cancel_swapchain(webgpu_swapchain swapchain,
                                              std::uint32_t* needs_recreate) noexcept {
  if (!open_ || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::cancel_swapchain(instance_, swapchain, needs_recreate);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_swapchain(webgpu_swapchain swapchain) noexcept {
  if (!open_ || swapchain == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_swapchain(instance_, swapchain);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

} // namespace granit::detail
