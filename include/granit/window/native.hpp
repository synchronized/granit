// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_NATIVE_HPP_
#define GRANIT_WINDOW_NATIVE_HPP_

#include <cstdint>

#include <granit/core/result.hpp>
#include <granit/window/native.h>

namespace granit {

[[nodiscard]] inline result native_win32(granit_window_system system, granit_window window,
                                         void*& instance, void*& native_window) noexcept {
  return from_native(granit_window_get_win32(system, window, &instance, &native_window));
}

[[nodiscard]] inline result native_xcb(granit_window_system system, granit_window window,
                                       void*& connection, std::uint32_t& native_window) noexcept {
  return from_native(granit_window_get_xcb(system, window, &connection, &native_window));
}

[[nodiscard]] inline result native_wayland(granit_window_system system, granit_window window,
                                           void*& display, void*& surface) noexcept {
  return from_native(granit_window_get_wayland(system, window, &display, &surface));
}

} // namespace granit

#endif
