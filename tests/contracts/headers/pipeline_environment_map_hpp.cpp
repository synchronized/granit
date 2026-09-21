// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/environment_map.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::environment_map>);
static_assert(std::is_move_constructible_v<granit::environment_map>);
