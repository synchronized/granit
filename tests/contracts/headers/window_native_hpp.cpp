// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/native.hpp>

using granit_window_native_win32_function = granit::result (*)(
    const granit::window_system&, granit::window_ref, granit::window_native_win32&) noexcept;

static_assert(static_cast<granit_window_native_win32_function>(&granit::get_native) != nullptr);
