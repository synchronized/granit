// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/timestamp_query.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::timestamp_query_pool>);
static_assert(std::is_move_constructible_v<granit::timestamp_query_pool>);
