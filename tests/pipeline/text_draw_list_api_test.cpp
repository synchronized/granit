// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_draw_list.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/text_atlas.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <limits>

namespace {
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable || value == granit::result::not_ready;
}

TEST_CASE("Text Draw List通过R8 Atlas批量生成Canvas四边形") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-text-canvas"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit_text_atlas_desc atlas_desc = GRANIT_TEXT_ATLAS_DESC_INIT;
  atlas_desc.page_width = 8;
  atlas_desc.page_height = 8;
  granit::text_atlas atlas;
  REQUIRE(atlas.initialize(renderer.native_handle(), atlas_desc) == granit::result::success);
  constexpr std::array<uint8_t, 4> bitmap{255, 128, 64, 0};
  granit_text_glyph_bitmap_desc bitmap_desc = GRANIT_TEXT_GLYPH_BITMAP_DESC_INIT;
  bitmap_desc.font_key = 7;
  bitmap_desc.glyph_id = 11;
  bitmap_desc.width = 2;
  bitmap_desc.height = 2;
  bitmap_desc.bearing_x = 1;
  bitmap_desc.bearing_y = 2;
  bitmap_desc.bitmap = bitmap.data();
  bitmap_desc.bitmap_size = bitmap.size();
  REQUIRE(atlas.upload_glyph(bitmap_desc) == granit::result::success);

  granit_text_draw_list_desc text_desc = GRANIT_TEXT_DRAW_LIST_DESC_INIT;
  granit::text_draw_list text;
  REQUIRE(text.initialize(renderer.native_handle(), text_desc) == granit::result::success);
  const std::array glyphs{
      granit_text_glyph_instance{7, 11, UINT32_C(0xff0000ff), 3, 5, {0, 0}},
      granit_text_glyph_instance{7, 11, UINT32_C(0xff00ff00), 6, 5, {0, 0}}};
  REQUIRE(text.append_glyph_run(glyphs, {0, 0, 16, 16}) == granit::result::success);

  granit_canvas_draw_list_desc canvas_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  granit::canvas_draw_list canvas;
  REQUIRE(canvas.initialize(renderer.native_handle(), canvas_desc) == granit::result::success);
  REQUIRE(text.append_to_canvas(atlas.native_handle(), canvas.native_handle()) ==
          granit::result::success);
  granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  REQUIRE(canvas.get_stats(stats) == granit::result::success);
  CHECK(stats.vertex_count == 8);
  CHECK(stats.index_count == 12);
  CHECK(stats.item_count == 2);
  CHECK(stats.batch_count == 1);

  REQUIRE(text.clear() == granit::result::success);
  const granit_text_glyph_instance missing{7, 99, UINT32_MAX, 0, 0, {0, 0}};
  REQUIRE(text.append_glyph_run(std::span{&missing, 1}) == granit::result::success);
  CHECK(text.append_to_canvas(atlas.native_handle(), canvas.native_handle()) ==
        granit::result::not_ready);
}
} // namespace

TEST_CASE("公共Text Draw List保存已整形字形并校验句柄") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-text-draw-list"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit_text_draw_list_desc desc = GRANIT_TEXT_DRAW_LIST_DESC_INIT;
  desc.initial_glyph_capacity = 8;
  desc.initial_run_capacity = 2;
  granit::text_draw_list list;
  REQUIRE(list.initialize(renderer.native_handle(), desc) == granit::result::success);
  const std::array glyphs{
      granit_text_glyph_instance{1, 42, UINT32_C(0xffffffff), 10, 20, {0, 0}},
      granit_text_glyph_instance{2, 0, UINT32_C(0xff00ffff), 18.5F, 20, {0, 0}}};
  REQUIRE(list.append_glyph_run(glyphs, {1, 2, 30, 40}) == granit::result::success);
  granit_text_draw_list_stats stats = GRANIT_TEXT_DRAW_LIST_STATS_INIT;
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.glyph_count == 2);
  CHECK(stats.run_count == 1);

  auto invalid = glyphs[0];
  invalid.font_key = 0;
  CHECK(list.append_glyph_run(std::span{&invalid, 1}) == granit::result::invalid_argument);
  invalid = glyphs[0];
  invalid.x = std::numeric_limits<float>::infinity();
  CHECK(list.append_glyph_run(std::span{&invalid, 1}) == granit::result::invalid_argument);
  CHECK(list.append_glyph_run(std::span{&glyphs[0], 1}, {1, 1, 0, 3}) ==
        granit::result::invalid_argument);
  CHECK(list.append_glyph_run({}) == granit::result::invalid_argument);

  REQUIRE(list.clear() == granit::result::success);
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.glyph_count == 0);
  CHECK(stats.run_count == 0);

  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-text-draw-second"}) ==
          granit::result::success);
  CHECK(granit_text_draw_list_get_stats(second.native_handle(), list.native_handle(), &stats) ==
        GRANIT_ERROR_INVALID_HANDLE);
  const auto old = list.native_handle();
  REQUIRE(list.destroy() == granit::result::success);
  CHECK(granit_text_draw_list_destroy(renderer.native_handle(), old) ==
        GRANIT_ERROR_INVALID_HANDLE);
}
