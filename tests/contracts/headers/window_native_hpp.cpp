// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/native.hpp>

#include <type_traits>

using granit_window_native_win32_function = granit::result (*)(
    const granit::window_system&, granit::window_ref, granit::window_native_win32&) noexcept;

static_assert(
    std::is_same_v<decltype(static_cast<granit_window_native_win32_function>(&granit::get_native)),
                   granit_window_native_win32_function>);
