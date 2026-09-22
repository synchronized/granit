// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/window.h>

#include "window/platform/backend.h"

#include <cstdlib>

using namespace granit::window::detail;

extern "C" granit_result granit_window_system_create(const granit_window_system_desc* desc,
                                                     granit_window_system* output) {
  if (output == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_WINDOW_SYSTEM_DESC_VERSION_1_SIZE ||
      desc->flags != 0 || desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32)
  if (desc->backend != GRANIT_WINDOW_BACKEND_AUTO && desc->backend != GRANIT_WINDOW_BACKEND_WIN32)
    return GRANIT_ERROR_UNSUPPORTED;
  return create_win32_system(output);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (desc->backend == GRANIT_WINDOW_BACKEND_WAYLAND ||
      (desc->backend == GRANIT_WINDOW_BACKEND_AUTO && std::getenv("WAYLAND_DISPLAY") != nullptr)) {
    const auto result = create_wayland_system(output);
    if (result == GRANIT_SUCCESS || desc->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
      return result;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (desc->backend != GRANIT_WINDOW_BACKEND_AUTO && desc->backend != GRANIT_WINDOW_BACKEND_XCB)
    return GRANIT_ERROR_UNSUPPORTED;
  return create_xcb_system(output);
#else
  return GRANIT_ERROR_BACKEND_UNAVAILABLE;
#endif
#elif defined(__EMSCRIPTEN__)
  if (desc->backend != GRANIT_WINDOW_BACKEND_AUTO &&
      desc->backend != GRANIT_WINDOW_BACKEND_EMSCRIPTEN)
    return GRANIT_ERROR_UNSUPPORTED;
  return create_emscripten_system(output);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_system_destroy(granit_window_system handle) {
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (system->loop_running)
    return GRANIT_ERROR_RESOURCE_IN_USE;
#if defined(_WIN32)
  return destroy_win32_system(handle, system);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return destroy_registered_wayland_system(handle, system);
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  return destroy_xcb_system(handle, system);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
#elif defined(__EMSCRIPTEN__)
  return destroy_emscripten_system(handle, system);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_system_process_events(granit_window_system handle) {
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32)
  return process_win32_events(system);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return process_wayland_events(system);
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (system->backend == GRANIT_WINDOW_BACKEND_XCB)
    return process_xcb_events(system);
#endif
  return GRANIT_ERROR_UNSUPPORTED;
#elif defined(__EMSCRIPTEN__)
  return process_emscripten_events(system);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}
