// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/core/shader_types.hpp>

#include <tuple>
#include <type_traits>

static_assert(std::is_same_v<granit::shader_content_id, granit::asset_content_id>);
static_assert(std::is_same_v<granit::shader_cache_key, granit::content_digest>);
static_assert(static_cast<std::uint32_t>(granit::shader_backend::all) ==
              GRANIT_SHADER_BACKEND_ALL_BITS);
