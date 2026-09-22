// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include <granit/renderer/texture.hpp>
#include <type_traits>

template <typename T>
concept resettable_texture_reference = requires(T& value) { value.reset(); };

static_assert(!std::is_copy_constructible_v<granit::texture>);
static_assert(!std::is_copy_constructible_v<granit::texture_view>);
static_assert(std::is_trivially_copyable_v<granit::texture_ref>);
static_assert(std::is_trivially_copyable_v<granit::texture_view_ref>);
static_assert(!resettable_texture_reference<granit::texture_ref>);
static_assert(!resettable_texture_reference<granit::texture_view_ref>);
