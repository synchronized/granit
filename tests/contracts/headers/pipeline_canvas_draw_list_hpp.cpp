// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/canvas_draw_list.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::canvas_draw_list>);
static_assert(std::is_move_constructible_v<granit::canvas_draw_list>);
static_assert(requires(granit::canvas_draw_list& list,
                       std::span<const granit::canvas_vertex> vertices,
                       std::span<const std::uint32_t> indices,
                       std::span<const granit::canvas_draw_range> ranges) {
  list.append_batch(vertices, indices, ranges);
});
static_assert(requires(granit::canvas_draw_list& list, granit::command_recorder& recorder,
                       const granit::canvas_rect_desc& rect,
                       const granit::canvas_record_desc& record,
                       granit::canvas_draw_list_stats& stats) {
  list.append_rect(rect);
  list.get_stats(stats);
  list.record(recorder, record);
  list.ref();
});
