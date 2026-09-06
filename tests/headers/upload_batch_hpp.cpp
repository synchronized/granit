// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/upload_batch.hpp>

#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<granit::upload_batch>);
static_assert(std::is_move_constructible_v<granit::upload_batch>);
static_assert(std::is_same_v<decltype(std::declval<granit::upload_batch&>().submit_async(
                                 std::declval<granit::async_operation&>())),
                             granit::result>);
