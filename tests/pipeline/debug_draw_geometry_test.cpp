// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/debug_draw_geometry.h"

#include <catch2/catch_all.hpp>

namespace {
granit_matrix4 identity() { return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; }
} // namespace

TEST_CASE("世界调试线段在Vulkan裁剪空间展开为恒定像素宽度") {
  using namespace granit::pipeline::detail;
  const granit_debug_draw_line line{
      {-0.5F, 0, 0.5F, UINT32_C(0xff0000ff)}, {0.5F, 0, 0.5F, UINT32_C(0xffff0000)}, 4,
      GRANIT_DEBUG_DRAW_SPACE_WORLD,          GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST,     0};
  std::array<debug_clip_vertex, 4> output{};
  REQUIRE(expand_world_debug_line(line, identity(), 100, 100, output) ==
          debug_line_expand_result::success);
  CHECK(output[0].position[1] == Catch::Approx(-0.04F));
  CHECK(output[1].position[1] == Catch::Approx(0.04F));
  CHECK(output[2].position[1] == Catch::Approx(-0.04F));
  CHECK(output[0].color == line.start.color);
  CHECK(output[2].color == line.end.color);
}

TEST_CASE("世界调试线段裁剪近面并拒绝完全不可见区间") {
  using namespace granit::pipeline::detail;
  granit_debug_draw_line line{
      {-0.5F, 0, -1, UINT32_C(0xff000000)}, {0.5F, 0, 1, UINT32_C(0xffffffff)}, 2,
      GRANIT_DEBUG_DRAW_SPACE_WORLD,        GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST,  0};
  std::array<debug_clip_vertex, 4> output{};
  REQUIRE(expand_world_debug_line(line, identity(), 200, 100, output) ==
          debug_line_expand_result::success);
  CHECK(output[0].position[2] == Catch::Approx(0));
  CHECK(output[0].position[0] == Catch::Approx(0));
  CHECK(output[0].color == UINT32_C(0xff808080));

  line.end.z = -0.25F;
  CHECK(expand_world_debug_line(line, identity(), 200, 100, output) ==
        debug_line_expand_result::clipped);
  CHECK(expand_world_debug_line(line, identity(), 0, 100, output) ==
        debug_line_expand_result::invalid_argument);
}
