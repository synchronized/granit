// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_draw_list.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::text_draw_list>);
static_assert(std::is_move_constructible_v<granit::text_draw_list>);
