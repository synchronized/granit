// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/async_operation.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::async_operation>);
static_assert(std::is_move_constructible_v<granit::async_operation>);
static_assert(std::is_nothrow_move_constructible_v<granit::async_operation>);
static_assert(!std::is_constructible_v<granit::async_operation, granit::renderer_ref,
                                       granit_async_operation>);
