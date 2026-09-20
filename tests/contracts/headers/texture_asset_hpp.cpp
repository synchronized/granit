// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include <granit/renderer/texture_asset.hpp>

#include <type_traits>

static_assert(std::is_same_v<granit::texture_content_id, granit::asset_content_id>);
granit::texture_asset_info granit_texture_asset_hpp_header_check() { return {}; }
