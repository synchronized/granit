// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/native_surface.h>
#include <granit/window/presentation.h>

#include "window/registry.h"

extern "C" granit_result granit_window_create_surface(granit_window_system system_handle,
                                                      granit_window window_handle,
                                                      granit_renderer renderer,
                                                      granit_surface* surface) {
  if (surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *surface = GRANIT_NULL_HANDLE;
  const auto system = granit::window::detail::acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!granit::window::detail::on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end() || renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;

  granit_surface_desc desc = GRANIT_SURFACE_DESC_INIT;
#if defined(_WIN32)
  desc.surface_type = GRANIT_SURFACE_TYPE_WIN32_BIT;
  desc.source.win32 = {found->second->instance, found->second->window};
#elif defined(__EMSCRIPTEN__)
  static constexpr char canvas_selector[] = "#canvas";
  desc.surface_type = GRANIT_SURFACE_TYPE_CANVAS_BIT;
  desc.source.canvas = {canvas_selector, static_cast<std::uint32_t>(sizeof(canvas_selector) - 1),
                        0};
#else
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND) {
    desc.surface_type = GRANIT_SURFACE_TYPE_WAYLAND_BIT;
    desc.source.wayland = {system->display, found->second->wayland_surface};
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (system->backend == GRANIT_WINDOW_BACKEND_XCB) {
    desc.surface_type = GRANIT_SURFACE_TYPE_XCB_BIT;
    desc.source.xcb = {system->connection, found->second->window, 0};
  }
#endif
#endif
  if (desc.surface_type == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  return granit_surface_create(renderer, &desc, surface);
}
