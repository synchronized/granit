// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::shader>);
static_assert(std::is_move_constructible_v<granit::shader>);
static_assert(std::is_trivially_copyable_v<granit::shader_ref>);
static_assert(granit::shader_ref::from_native(UINT64_C(7)).native_handle() == UINT64_C(7));
