// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_draw_list.hpp>

#include <span>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::text_draw_list>);
static_assert(std::is_move_constructible_v<granit::text_draw_list>);
static_assert(requires(granit::text_draw_list& list, granit::renderer& renderer,
                       const granit::text_draw_list_desc& desc,
                       std::span<const granit::text_glyph_instance> glyphs,
                       granit::text_draw_list_stats& stats, granit::text_atlas_ref atlas,
                       granit::canvas_draw_list_ref canvas) {
  list.initialize(renderer, desc);
  list.append_glyph_run(glyphs);
  list.get_stats(stats);
  list.append_to_canvas(atlas, canvas);
  list.ref();
});
