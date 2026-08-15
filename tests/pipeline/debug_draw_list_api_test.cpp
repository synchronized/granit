// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/debug_draw_list.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <limits>

namespace {
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable || value == granit::result::not_ready;
}
} // namespace

TEST_CASE("公共Debug Draw List支持批量命令、复用和句柄失效") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-debug-draw-list"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit_debug_draw_list_desc desc = GRANIT_DEBUG_DRAW_LIST_DESC_INIT;
  desc.initial_line_capacity = 4;
  desc.initial_triangle_capacity = 2;
  granit::debug_draw_list list;
  REQUIRE(list.initialize(renderer.native_handle(), desc) == granit::result::success);

  const std::array lines{granit_debug_draw_line{{0, 0, 0, UINT32_MAX},
                                                {1, 1, 1, UINT32_MAX},
                                                1,
                                                GRANIT_DEBUG_DRAW_SPACE_WORLD,
                                                GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST,
                                                0},
                         granit_debug_draw_line{{2, 3, 0, UINT32_MAX},
                                                {4, 5, 0, UINT32_MAX},
                                                2,
                                                GRANIT_DEBUG_DRAW_SPACE_SCREEN,
                                                GRANIT_DEBUG_DRAW_DEPTH_MODE_DISABLED,
                                                0}};
  const std::array triangles{granit_debug_draw_triangle{
      {{0, 0, 0, UINT32_MAX}, {1, 0, 0, UINT32_MAX}, {0, 1, 0, UINT32_MAX}},
      GRANIT_DEBUG_DRAW_SPACE_WORLD,
      GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST,
      {0, 0}}};
  REQUIRE(list.append_lines(lines) == granit::result::success);
  REQUIRE(list.append_triangles(triangles) == granit::result::success);
  granit_debug_draw_list_stats stats = GRANIT_DEBUG_DRAW_LIST_STATS_INIT;
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.line_count == 2);
  CHECK(stats.triangle_count == 1);
  REQUIRE(list.clear() == granit::result::success);
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.line_count == 0);
  CHECK(stats.triangle_count == 0);

  auto invalid = lines[0];
  invalid.width = 0;
  CHECK(list.append_lines(std::span{&invalid, 1}) == granit::result::invalid_argument);
  invalid = lines[0];
  invalid.start.x = std::numeric_limits<float>::infinity();
  CHECK(list.append_lines(std::span{&invalid, 1}) == granit::result::invalid_argument);
  invalid = lines[0];
  invalid.space = GRANIT_DEBUG_DRAW_SPACE_SCREEN;
  CHECK(list.append_lines(std::span{&invalid, 1}) == granit::result::invalid_argument);

  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-debug-draw-second"}) ==
          granit::result::success);
  granit_debug_draw_list_stats cross_stats = GRANIT_DEBUG_DRAW_LIST_STATS_INIT;
  CHECK(granit_debug_draw_list_get_stats(second.native_handle(), list.native_handle(),
                                         &cross_stats) == GRANIT_ERROR_INVALID_HANDLE);
  const auto old = list.native_handle();
  REQUIRE(list.destroy() == granit::result::success);
  CHECK(granit_debug_draw_list_destroy(renderer.native_handle(), old) ==
        GRANIT_ERROR_INVALID_HANDLE);
}
