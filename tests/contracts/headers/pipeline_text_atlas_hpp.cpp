// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_atlas.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::text_atlas>);
static_assert(std::is_move_constructible_v<granit::text_atlas>);
static_assert(requires(granit::text_atlas& atlas, granit::renderer& renderer,
                       const granit::text_atlas_desc& desc,
                       const granit::text_glyph_bitmap_desc& glyph,
                       granit::text_atlas_stats& stats) {
  atlas.initialize(renderer, desc);
  atlas.upload_glyph(glyph);
  atlas.get_stats(stats);
  atlas.ref();
});
