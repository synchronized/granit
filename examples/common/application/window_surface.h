// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLE_APPLICATION_WINDOW_SURFACE_H_
#define GRANIT_EXAMPLE_APPLICATION_WINDOW_SURFACE_H_

#include <granit/integrations/sdl3/surface.hpp>
#include <granit/renderer/native_surface.hpp>
#include <granit/window/native.hpp>

namespace granit::example {

inline result create_window_surface(renderer_ref renderer, const window_system& system,
                                    window_ref window, surface& output) noexcept {
  window_native native = GRANIT_WINDOW_NATIVE_INIT;
  const auto native_result = get_native(system, window, native);
  if (native_result.failed())
    return native_result;

  switch (native.backend) {
  case GRANIT_WINDOW_BACKEND_WIN32:
    return output.initialize(renderer,
                             surface_desc::win32(native.u.win32.instance, native.u.win32.window));
  case GRANIT_WINDOW_BACKEND_XCB:
    return output.initialize(renderer,
                             surface_desc::xcb(native.u.xcb.connection, native.u.xcb.window));
  case GRANIT_WINDOW_BACKEND_WAYLAND:
    return output.initialize(renderer,
                             surface_desc::wayland(native.u.wayland.display,
                                                    native.u.wayland.surface));
  case GRANIT_WINDOW_BACKEND_EMSCRIPTEN:
    return output.initialize(
        renderer,
        surface_desc::canvas({native.u.emscripten.canvas_selector,
                              native.u.emscripten.canvas_selector_length}));
  case GRANIT_WINDOW_BACKEND_SDL3:
    return integration::sdl3::create_surface(
        renderer, static_cast<SDL_Window*>(native.u.sdl3.window), output);
  default:
    return result::unsupported;
  }
}

} // namespace granit::example

#endif
