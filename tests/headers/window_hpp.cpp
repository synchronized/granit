// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.hpp>

#include <type_traits>

static_assert(std::is_standard_layout_v<granit::window_desc>);
static_assert(sizeof(granit::window_event) == sizeof(granit_window_event));
static_assert(std::is_move_constructible_v<granit::window_system>);
static_assert(!std::is_copy_constructible_v<granit::window_system>);
static_assert(std::is_move_constructible_v<granit::window>);
static_assert(!std::is_copy_constructible_v<granit::window>);
static_assert(static_cast<std::uint32_t>(granit::window_backend::win32) ==
              GRANIT_WINDOW_BACKEND_WIN32);
