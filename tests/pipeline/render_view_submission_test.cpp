// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/render_view_submission.h"

#include <array>
#include <catch2/catch_all.hpp>

namespace {

granit_scene_renderable renderable(float z, std::uint64_t sort_key, std::uint32_t object_id) {
  auto value = granit_scene_renderable{};
  value.bounds_center = {0, 0, z};
  value.sort_key = sort_key;
  value.object_id = object_id;
  return value;
}

} // namespace

TEST_CASE("透明 PBR Draw 按距离与稳定键排序") {
  granit::pipeline::detail::render_view_submission input;
  input.view.camera_position = {0, 0, 0};
  input.draw_bindings.resize(5);
  input.renderables = {
      renderable(2, 5, 5),  renderable(10, 8, 8), renderable(10, 2, 9),
      renderable(10, 2, 3), renderable(10, 2, 3),
  };
  input.pbr_objects.resize(5);
  for (std::size_t index = 0; index < input.draw_bindings.size(); ++index)
    input.draw_bindings[index].variant = index + 1;

  constexpr std::array<std::uint8_t, 5> transparent{0, 1, 1, 1, 1};
  granit::pipeline::detail::forward_draw_submission opaque;
  granit::pipeline::detail::forward_draw_submission blended;
  REQUIRE(granit::pipeline::detail::partition_forward_draws(input, transparent, opaque, blended) ==
          GRANIT_SUCCESS);

  REQUIRE(opaque.source_indices == std::vector<std::size_t>{0});
  REQUIRE(blended.source_indices == std::vector<std::size_t>{3, 4, 2, 1});
  REQUIRE(blended.draw_bindings.size() == 4);
  CHECK(blended.draw_bindings[0].variant == 4);
  CHECK(blended.draw_bindings[1].variant == 5);
}

TEST_CASE("透明 PBR Draw 分类拒绝不一致输入") {
  granit::pipeline::detail::render_view_submission input;
  input.draw_bindings.resize(1);
  granit::pipeline::detail::forward_draw_submission opaque;
  granit::pipeline::detail::forward_draw_submission blended;
  constexpr std::array<std::uint8_t, 1> transparent{1};
  CHECK(granit::pipeline::detail::partition_forward_draws(input, transparent, opaque, blended) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}
