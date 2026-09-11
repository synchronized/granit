// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader_library.hpp>

#include <tuple>
#include <type_traits>

static_assert(std::is_same_v<granit::shader_content_id, granit::shader_digest>);
static_assert(std::is_same_v<granit::shader_cache_key, granit::shader_digest>);
static_assert(std::tuple_size_v<granit::shader_digest> == GRANIT_SHADER_DIGEST_SIZE);
