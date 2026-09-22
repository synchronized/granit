// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/native.h>
#include <granit/window/window.h>

#include "window/platform/backend.h"

#include <utility>

using namespace granit::window::detail;

extern "C" granit_result granit_window_create(granit_window_system system_handle,
                                              const granit_window_desc* desc,
                                              granit_window* output) {
  if (output == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_WINDOW_DESC_VERSION_1_SIZE ||
      desc->width == 0 || desc->height == 0 || desc->reserved != 0 ||
      (desc->flags & ~(GRANIT_WINDOW_VISIBLE_BIT | GRANIT_WINDOW_RESIZABLE_BIT |
                       GRANIT_WINDOW_HIGH_DPI_BIT)) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32)
  return create_win32_window(system, desc, output);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return create_wayland_window(system, desc, output);
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  return create_xcb_window(system, desc, output);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
#elif defined(__EMSCRIPTEN__)
  return create_emscripten_window(system, desc, output);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_destroy(granit_window_system system_handle,
                                               granit_window window_handle) {
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  clear_window_input(*system, window_handle);
#if defined(_WIN32)
  return destroy_win32_window(system, window_handle);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
  const auto window = std::move(found->second);
  system->windows.erase(found);
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return destroy_registered_wayland_window(system, window, window_handle);
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  return destroy_xcb_window(system, window);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
#elif defined(__EMSCRIPTEN__)
  return destroy_emscripten_window(system, window_handle);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_get_state(granit_window_system system_handle,
                                                 granit_window window_handle,
                                                 granit_window_state* state) {
  if (state == nullptr || state->struct_size < GRANIT_WINDOW_STATE_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *state = GRANIT_WINDOW_STATE_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& window = *found->second;
  state->width = window.width;
  state->height = window.height;
  state->framebuffer_width = window.framebuffer_width;
  state->framebuffer_height = window.framebuffer_height;
  state->content_scale_horizontal = window.content_scale_horizontal;
  state->content_scale_vertical = window.content_scale_vertical;
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_window_get_native_win32(granit_window_system system_handle,
                                                        granit_window window_handle,
                                                        granit_window_native_win32* output) {
  if (output == nullptr || output->struct_size < GRANIT_WINDOW_NATIVE_WIN32_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_WINDOW_NATIVE_WIN32_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(_WIN32)
  return get_native_win32(found->second, *output);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_get_native_xcb(granit_window_system system_handle,
                                                      granit_window window_handle,
                                                      granit_window_native_xcb* output) {
  if (output == nullptr || output->struct_size < GRANIT_WINDOW_NATIVE_XCB_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_WINDOW_NATIVE_XCB_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (system->backend == GRANIT_WINDOW_BACKEND_XCB)
    return get_native_xcb(system, found->second, *output);
#endif
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_window_get_native_wayland(granit_window_system system_handle,
                                                          granit_window window_handle,
                                                          granit_window_native_wayland* output) {
  if (output == nullptr || output->struct_size < GRANIT_WINDOW_NATIVE_WAYLAND_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_WINDOW_NATIVE_WAYLAND_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return get_native_wayland(system, found->second, *output);
#endif
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result
granit_window_get_native_emscripten(granit_window_system system_handle, granit_window window_handle,
                                    granit_window_native_emscripten* output) {
  if (output == nullptr || output->struct_size < GRANIT_WINDOW_NATIVE_EMSCRIPTEN_VERSION_1_SIZE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *output = GRANIT_WINDOW_NATIVE_EMSCRIPTEN_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(__EMSCRIPTEN__)
  return get_native_emscripten(*output);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}
