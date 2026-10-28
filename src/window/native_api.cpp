// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/native.h>
#include <granit/window/window.h>

#include "window/platform/backend.h"

using namespace granit::window::detail;

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
  if (system->backend == GRANIT_WINDOW_BACKEND_WIN32)
    return get_native_win32(found->second, *output);
#endif
  return GRANIT_ERROR_UNSUPPORTED;
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
  if (system->backend == GRANIT_WINDOW_BACKEND_EMSCRIPTEN)
    return get_native_emscripten(found->second, *output);
#endif
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result
granit_window_get_native_sdl3(granit_window_system system_handle, granit_window window_handle,
                              granit_window_native_sdl3* output) {
  if (output == nullptr || output->struct_size < GRANIT_WINDOW_NATIVE_SDL3_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_WINDOW_NATIVE_SDL3_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(GRANIT_WINDOW_HAS_SDL3)
  if (system->backend == GRANIT_WINDOW_BACKEND_SDL3)
    return get_native_sdl3(found->second, *output);
#endif
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result
granit_window_get_native(granit_window_system system_handle, granit_window window_handle,
                                    granit_window_native* output) {
  if (output == nullptr || output->struct_size < GRANIT_WINDOW_NATIVE_VERSION_1_SIZE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *output = GRANIT_WINDOW_NATIVE_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;

  granit_result result = GRANIT_ERROR_UNSUPPORTED;
#if defined(_WIN32)
  if (system->backend == GRANIT_WINDOW_BACKEND_WIN32) {
    granit_window_native_win32 win32 = GRANIT_WINDOW_NATIVE_WIN32_INIT;
    result = get_native_win32(found->second, win32);
    if (result == GRANIT_SUCCESS) {
      output->struct_size = GRANIT_WINDOW_NATIVE_VERSION_1_SIZE;
      output->backend = system->backend;
      output->u.win32 = win32;
    }
    return result;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND) {
    granit_window_native_wayland wayland = GRANIT_WINDOW_NATIVE_WAYLAND_INIT;
    result = get_native_wayland(system, found->second, wayland);
    if (result == GRANIT_SUCCESS) {
      output->struct_size = GRANIT_WINDOW_NATIVE_VERSION_1_SIZE;
      output->backend = system->backend;
      output->u.wayland = wayland;
    }
    return result;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (system->backend == GRANIT_WINDOW_BACKEND_XCB) {
    granit_window_native_xcb xcb = GRANIT_WINDOW_NATIVE_XCB_INIT;
    result = get_native_xcb(system, found->second, xcb);
    if (result == GRANIT_SUCCESS) {
      output->struct_size = GRANIT_WINDOW_NATIVE_VERSION_1_SIZE;
      output->backend = system->backend;
      output->u.xcb = xcb;
    }
    return result;
  }
#endif
#if defined(__EMSCRIPTEN__)
  if (system->backend == GRANIT_WINDOW_BACKEND_EMSCRIPTEN) {
    granit_window_native_emscripten emscripten = GRANIT_WINDOW_NATIVE_EMSCRIPTEN_INIT;
    result = get_native_emscripten(found->second, emscripten);
    if (result == GRANIT_SUCCESS) {
      output->struct_size = GRANIT_WINDOW_NATIVE_VERSION_1_SIZE;
      output->backend = system->backend;
      output->u.emscripten = emscripten;
    }
    return result;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_SDL3)
  if (system->backend == GRANIT_WINDOW_BACKEND_SDL3) {
    granit_window_native_sdl3 sdl3 = GRANIT_WINDOW_NATIVE_SDL3_INIT;
    result = get_native_sdl3(found->second, sdl3);
    if (result == GRANIT_SUCCESS) {
      output->struct_size = GRANIT_WINDOW_NATIVE_VERSION_1_SIZE;
      output->backend = system->backend;
      output->u.sdl3 = sdl3;
    }
    return result;
  }
#endif
  return result;
}
