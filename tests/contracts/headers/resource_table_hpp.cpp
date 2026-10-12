// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/resource_table.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::resource_table>);
static_assert(std::is_move_constructible_v<granit::resource_table>);

int granit_resource_table_cpp_header_test() { return 0; }
