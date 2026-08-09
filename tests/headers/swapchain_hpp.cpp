// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/swapchain.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<granit::swapchain>);
static_assert(!std::is_copy_constructible_v<granit::swapchain>);
static_assert(std::is_move_constructible_v<granit::acquired_frame>);
static_assert(!std::is_copy_constructible_v<granit::acquired_frame>);
