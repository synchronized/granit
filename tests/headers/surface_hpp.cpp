// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/surface.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<granit::surface>);
static_assert(!std::is_copy_constructible_v<granit::surface>);
static_assert(sizeof(granit::xcb_surface_desc::window) == sizeof(std::uint32_t));
static_assert(std::is_same_v<decltype(granit::canvas_surface_desc::selector), std::string_view>);
