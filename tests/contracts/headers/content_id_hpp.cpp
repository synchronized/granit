// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/core/content_id.hpp>

#include <tuple>
#include <type_traits>

static_assert(std::is_same_v<granit::asset_content_id, granit::content_digest>);
static_assert(std::tuple_size_v<granit::content_digest> == GRANIT_CONTENT_DIGEST_SIZE);

granit::asset_content_id granit_content_id_hpp_header_check() { return {}; }
