// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::shader>);
static_assert(std::is_move_constructible_v<granit::shader>);
