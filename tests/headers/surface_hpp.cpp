// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/surface.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<granit::surface>);
static_assert(!std::is_copy_constructible_v<granit::surface>);
static_assert(std::is_same_v<decltype(granit::surface_desc::xcb(nullptr, std::uint32_t{})),
                             granit::surface_desc>);
static_assert(std::is_same_v<decltype(granit::surface_desc::canvas(std::string_view{})),
                             granit::surface_desc>);
