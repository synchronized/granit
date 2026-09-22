// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_NATIVE_HPP_
#define GRANIT_WINDOW_NATIVE_HPP_

#include <granit/core/result.hpp>
#include <granit/window/native.h>
#include <granit/window/window.hpp>

namespace granit {

using window_native_win32 = granit_window_native_win32;
using window_native_xcb = granit_window_native_xcb;
using window_native_wayland = granit_window_native_wayland;
using window_native_emscripten = granit_window_native_emscripten;

[[nodiscard]] inline result get_native(const window_system& system, const window& value,
                                       window_native_win32& output) noexcept {
  output = GRANIT_WINDOW_NATIVE_WIN32_INIT;
  return from_native(
      granit_window_get_native_win32(system.native_handle(), value.native_handle(), &output));
}

[[nodiscard]] inline result get_native(const window_system& system, const window& value,
                                       window_native_xcb& output) noexcept {
  output = GRANIT_WINDOW_NATIVE_XCB_INIT;
  return from_native(
      granit_window_get_native_xcb(system.native_handle(), value.native_handle(), &output));
}

[[nodiscard]] inline result get_native(const window_system& system, const window& value,
                                       window_native_wayland& output) noexcept {
  output = GRANIT_WINDOW_NATIVE_WAYLAND_INIT;
  return from_native(
      granit_window_get_native_wayland(system.native_handle(), value.native_handle(), &output));
}

[[nodiscard]] inline result get_native(const window_system& system, const window& value,
                                       window_native_emscripten& output) noexcept {
  output = GRANIT_WINDOW_NATIVE_EMSCRIPTEN_INIT;
  return from_native(
      granit_window_get_native_emscripten(system.native_handle(), value.native_handle(), &output));
}

} // namespace granit

#endif
