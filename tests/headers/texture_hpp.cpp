// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include <granit/renderer/texture.hpp>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<granit::texture>);
static_assert(!std::is_copy_constructible_v<granit::texture_view>);
