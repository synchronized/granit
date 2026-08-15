// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_draw_list.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <limits>

namespace {
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable || value == granit::result::not_ready;
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
