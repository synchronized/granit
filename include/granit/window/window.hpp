// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_WINDOW_HPP_
#define GRANIT_WINDOW_WINDOW_HPP_

#include <cstdint>

#include <granit/core/result.hpp>
#include <granit/window/window.h>

namespace granit {

enum class window_backend : std::uint32_t {
  automatic = GRANIT_WINDOW_BACKEND_AUTO,
  win32 = GRANIT_WINDOW_BACKEND_WIN32,
  xcb = GRANIT_WINDOW_BACKEND_XCB,
  wayland = GRANIT_WINDOW_BACKEND_WAYLAND
};

struct window_system_desc {
  window_backend backend{window_backend::automatic};
};

struct window_desc {
  const char* title{};
  std::uint32_t title_length{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t flags{GRANIT_WINDOW_VISIBLE_BIT | GRANIT_WINDOW_RESIZABLE_BIT};
};

using window_event = granit_window_event;

} // namespace granit

#endif
