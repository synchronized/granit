// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/integrations/sdl3/surface.hpp>
#include <granit/renderer/native_surface.hpp>

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#if defined(GRANIT_INTEGRATION_SDL3_HAS_X11)
#include <X11/Xlib-xcb.h>
#endif

#include <cstdint>
#include <cstring>
#include <limits>

namespace granit::integration::sdl3 {
namespace {

const char* video_driver() noexcept {
  const auto* driver = SDL_GetCurrentVideoDriver();
  return driver == nullptr ? "" : driver;
}

} // namespace

result create_surface(granit_renderer renderer, SDL_Window* window, surface& output) noexcept {
  if (window == nullptr)
    return result::invalid_argument;
  const auto properties = SDL_GetWindowProperties(window);
  if (properties == 0)
    return result::invalid_argument;
  const auto* driver = video_driver();
  if (std::strcmp(driver, "windows") == 0) {
    auto* instance =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
    auto* native_window =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (instance == nullptr || native_window == nullptr)
      return result::backend_unavailable;
    return output.initialize(renderer, granit::surface_desc::win32(instance, native_window));
  }
  if (std::strcmp(driver, "wayland") == 0) {
    auto* display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    auto* native_surface =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (display == nullptr || native_surface == nullptr)
      return result::backend_unavailable;
    return output.initialize(renderer, granit::surface_desc::wayland(display, native_surface));
  }
#if defined(GRANIT_INTEGRATION_SDL3_HAS_X11)
  if (std::strcmp(driver, "x11") != 0)
    return result::unsupported;
  auto* display = static_cast<Display*>(
      SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
  const auto native_window =
      SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
  if (display == nullptr || native_window <= 0 ||
      static_cast<std::uint64_t>(native_window) > std::numeric_limits<std::uint32_t>::max())
    return result::backend_unavailable;
  auto* connection = XGetXCBConnection(display);
  if (connection == nullptr)
    return result::backend_unavailable;
  return output.initialize(
      renderer, surface_desc::xcb(connection, static_cast<std::uint32_t>(native_window)));
#else
  return result::unsupported;
#endif
}

} // namespace granit::integration::sdl3
