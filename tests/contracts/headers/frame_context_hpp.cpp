// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/frame_context.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::frame_context>);
static_assert(std::is_move_constructible_v<granit::frame_context>);
static_assert(!std::is_copy_constructible_v<granit::frame_recording>);
static_assert(std::is_move_constructible_v<granit::frame_recording>);
