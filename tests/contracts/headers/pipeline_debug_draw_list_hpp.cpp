// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/debug_draw_list.hpp>

#include <span>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::debug_draw_list>);
static_assert(std::is_move_constructible_v<granit::debug_draw_list>);
static_assert(requires(granit::debug_draw_list& list,
                       std::span<const granit::debug_draw_line> lines,
                       std::span<const granit::debug_draw_triangle> triangles,
                       granit::debug_draw_list_stats& stats) {
  list.append_lines(lines);
  list.append_triangles(triangles);
  list.get_stats(stats);
  list.ref();
});
static_assert(requires(granit::debug_draw_list& list, granit::command_recorder& recorder,
                       granit::canvas_draw_list_ref canvas,
                       const granit::debug_draw_record_desc& record) {
  list.append_screen_to_canvas(canvas);
  list.record_world(recorder, record);
});
