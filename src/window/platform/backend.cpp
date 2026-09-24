// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "window/platform/backend.h"

#include <cstdlib>
#include <utility>

namespace granit::window::detail {
namespace {

#if defined(_WIN32)
const backend_operations win32_operations{destroy_win32_system,
                                          process_win32_events,
                                          create_win32_window,
                                          destroy_win32_window,
                                          nullptr,
                                          false};
#endif

#if defined(GRANIT_WINDOW_HAS_XCB)
granit_result destroy_xcb_window_dispatch(const std::shared_ptr<window_system_record>& system,
                                          granit_window handle) {
  const auto found = system->windows.find(handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto window = std::move(found->second);
  system->windows.erase(found);
  return destroy_xcb_window(system, window);
}

const backend_operations xcb_operations{destroy_xcb_system,
                                        process_xcb_events,
                                        create_xcb_window,
                                        destroy_xcb_window_dispatch,
                                        nullptr,
                                        false};
#endif

#if defined(GRANIT_WINDOW_HAS_WAYLAND)
granit_result destroy_wayland_window_dispatch(const std::shared_ptr<window_system_record>& system,
                                              granit_window handle) {
  const auto found = system->windows.find(handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto window = std::move(found->second);
  system->windows.erase(found);
  return destroy_registered_wayland_window(system, window, handle);
}

const backend_operations wayland_operations{destroy_registered_wayland_system,
                                            process_wayland_events,
                                            create_wayland_window,
                                            destroy_wayland_window_dispatch,
                                            nullptr,
                                            false};
#endif

#if defined(__EMSCRIPTEN__)
const backend_operations emscripten_operations{destroy_emscripten_system,
                                               process_emscripten_events,
                                               create_emscripten_window,
                                               destroy_emscripten_window,
                                               nullptr,
                                               true};
#endif

#if defined(GRANIT_WINDOW_HAS_SDL3)
const backend_operations sdl3_operations{destroy_sdl3_system, process_sdl3_events,
                                         create_sdl3_window,  destroy_sdl3_window,
                                         create_sdl3_surface, true};
#endif

granit_result initialize(const backend_operations& operations,
                         granit_result (*create_system)(granit_window_system*),
                         granit_window_system* output) {
  const auto result = create_system(output);
  if (result != GRANIT_SUCCESS)
    return result;
  const auto system = acquire_system(*output);
  if (system == nullptr)
    return GRANIT_ERROR_INTERNAL;
  system->operations = &operations;
  return GRANIT_SUCCESS;
}

} // namespace

granit_result create_backend_system(std::uint32_t requested_backend, granit_window_system* output) {
#if defined(GRANIT_WINDOW_HAS_SDL3)
  if (requested_backend == GRANIT_WINDOW_BACKEND_SDL3)
    return initialize(sdl3_operations, create_sdl3_system, output);
#else
  if (requested_backend == GRANIT_WINDOW_BACKEND_SDL3)
    return GRANIT_ERROR_UNSUPPORTED;
#endif

#if defined(_WIN32)
  if (requested_backend != GRANIT_WINDOW_BACKEND_AUTO &&
      requested_backend != GRANIT_WINDOW_BACKEND_WIN32) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return initialize(win32_operations, create_win32_system, output);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (requested_backend == GRANIT_WINDOW_BACKEND_WAYLAND ||
      (requested_backend == GRANIT_WINDOW_BACKEND_AUTO &&
       std::getenv("WAYLAND_DISPLAY") != nullptr)) {
    const auto result = initialize(wayland_operations, create_wayland_system, output);
    if (result == GRANIT_SUCCESS || requested_backend == GRANIT_WINDOW_BACKEND_WAYLAND)
      return result;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (requested_backend != GRANIT_WINDOW_BACKEND_AUTO &&
      requested_backend != GRANIT_WINDOW_BACKEND_XCB) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return initialize(xcb_operations, create_xcb_system, output);
#else
  return GRANIT_ERROR_BACKEND_UNAVAILABLE;
#endif
#elif defined(__EMSCRIPTEN__)
  if (requested_backend != GRANIT_WINDOW_BACKEND_AUTO &&
      requested_backend != GRANIT_WINDOW_BACKEND_EMSCRIPTEN) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return initialize(emscripten_operations, create_emscripten_system, output);
#else
  static_cast<void>(requested_backend);
  static_cast<void>(output);
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

} // namespace granit::window::detail
