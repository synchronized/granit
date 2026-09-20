// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/native.hpp>

#include <type_traits>

static_assert(
    std::is_same_v<decltype(granit::native_win32),
                   granit::result(granit_window_system, granit_window, void*&, void*&) noexcept>);
