// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_NATIVE_HPP_
#define GRANIT_WINDOW_NATIVE_HPP_

#include <granit/core/result.hpp>
#include <granit/window/native.h>
#include <granit/window/window.hpp>

namespace granit {

/** C++ 原生窗口快照；构造时自动填充 struct_size 和默认值。 */
struct window_native_win32 : granit_window_native_win32 {
  constexpr window_native_win32() noexcept
      : granit_window_native_win32(GRANIT_WINDOW_NATIVE_WIN32_INIT) {}
};

struct window_native_xcb : granit_window_native_xcb {
  constexpr window_native_xcb() noexcept
      : granit_window_native_xcb(GRANIT_WINDOW_NATIVE_XCB_INIT) {}
};

struct window_native_wayland : granit_window_native_wayland {
  constexpr window_native_wayland() noexcept
      : granit_window_native_wayland(GRANIT_WINDOW_NATIVE_WAYLAND_INIT) {}
};

struct window_native_emscripten : granit_window_native_emscripten {
  constexpr window_native_emscripten() noexcept
      : granit_window_native_emscripten(GRANIT_WINDOW_NATIVE_EMSCRIPTEN_INIT) {}
};

struct window_native_sdl3 : granit_window_native_sdl3 {
  constexpr window_native_sdl3() noexcept
      : granit_window_native_sdl3(GRANIT_WINDOW_NATIVE_SDL3_INIT) {}
};

struct window_native : granit_window_native {
  constexpr window_native() noexcept : granit_window_native(GRANIT_WINDOW_NATIVE_INIT) {}
};

[[nodiscard]] inline result get_native(const window_system& system, window_ref value,
                                       window_native& output) noexcept {
  output = window_native{};
  return from_native(
      granit_window_get_native(system.native_handle(), value.native_handle(), &output));
}

[[nodiscard]] inline result get_native(const window_system& system, window_ref value,
                                       window_native_win32& output) noexcept {
  output = window_native_win32{};
  return from_native(
      granit_window_get_native_win32(system.native_handle(), value.native_handle(), &output));
}

[[nodiscard]] inline result get_native(const window_system& system, window_ref value,
                                       window_native_xcb& output) noexcept {
  output = window_native_xcb{};
  return from_native(
      granit_window_get_native_xcb(system.native_handle(), value.native_handle(), &output));
}

[[nodiscard]] inline result get_native(const window_system& system, window_ref value,
                                       window_native_wayland& output) noexcept {
  output = window_native_wayland{};
  return from_native(
      granit_window_get_native_wayland(system.native_handle(), value.native_handle(), &output));
}

[[nodiscard]] inline result get_native(const window_system& system, window_ref value,
                                       window_native_emscripten& output) noexcept {
  output = window_native_emscripten{};
  return from_native(
      granit_window_get_native_emscripten(system.native_handle(), value.native_handle(), &output));
}

[[nodiscard]] inline result get_native(const window_system& system, window_ref value,
                                       window_native_sdl3& output) noexcept {
  output = window_native_sdl3{};
  return from_native(
      granit_window_get_native_sdl3(system.native_handle(), value.native_handle(), &output));
}

} // namespace granit

#endif
