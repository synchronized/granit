// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.hpp>

#include <type_traits>

static_assert(std::is_standard_layout_v<granit::window_desc>);
static_assert(std::is_same_v<decltype(granit::window_event{}.type), granit::window_event_type>);
static_assert(sizeof(granit::window_event) == sizeof(granit_window_event));
static_assert(std::is_same_v<decltype(granit::window_event{}.data.focus.focused), bool>);
static_assert(std::is_same_v<decltype(granit::window_event{}.data.native_handle.backend),
                             granit::window_backend>);
static_assert(std::is_same_v<decltype(granit::window_desc{}.flags), granit::window_flag>);
static_assert(std::is_same_v<decltype(granit::window_desc{}.target), granit::window_target>);
static_assert(std::is_move_constructible_v<granit::window_system>);
static_assert(!std::is_copy_constructible_v<granit::window_system>);
static_assert(std::is_move_constructible_v<granit::window>);
static_assert(!std::is_copy_constructible_v<granit::window>);
static_assert(static_cast<std::uint32_t>(granit::window_backend::win32) ==
              GRANIT_WINDOW_BACKEND_WIN32);
static_assert(static_cast<std::uint32_t>(granit::window_backend::emscripten) ==
              GRANIT_WINDOW_BACKEND_EMSCRIPTEN);
static_assert(granit::window_target::canvas("#viewport").type ==
              granit::window_target_type::canvas_selector);
