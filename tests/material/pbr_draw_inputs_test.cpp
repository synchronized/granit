// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_draw_inputs.h"

#include <catch2/catch_all.hpp>

#include <limits>

namespace {

constexpr granit::material::pbr_matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

} // namespace

TEST_CASE("PBR 显式 Draw 输入规范化方向光并打包固定布局") {
  const granit::material::pbr_view_input view{.view_projection = identity,
                                              .camera_position = {1.0F, 2.0F, 3.0F}};
  const granit::material::pbr_object_input object{
      .model = identity, .normal_matrix = identity, .object_id = 42};
  const granit::material::pbr_directional_light_input light{
      .direction_to_light = {0.0F, 0.0F, 2.0F}, .radiance = {4.0F, 3.0F, 2.0F}};
  granit::material::pbr_frame_constants frame{};
  granit::material::pbr_object_constants object_constants{};
  REQUIRE(granit::material::pack_pbr_draw_inputs(view, object, light, frame, object_constants) ==
          granit::material::pbr_draw_input_error::none);
  CHECK(frame.view_projection == identity);
  CHECK(frame.camera_position == std::array{1.0F, 2.0F, 3.0F, 0.0F});
  CHECK(frame.direction_to_light == std::array{0.0F, 0.0F, 1.0F, 0.0F});
  CHECK(frame.light_radiance == std::array{4.0F, 3.0F, 2.0F, 0.0F});
  CHECK(object_constants.object_id == std::array<std::uint32_t, 4>{42, 0, 0, 0});
}

TEST_CASE("PBR 显式 Draw 输入拒绝无效方向光") {
  granit::material::pbr_view_input view{.view_projection = identity};
  const granit::material::pbr_object_input object{.model = identity, .normal_matrix = identity};
  granit::material::pbr_directional_light_input light{.direction_to_light = {}};
  granit::material::pbr_frame_constants frame{};
  granit::material::pbr_object_constants object_constants{};
  CHECK(granit::material::pack_pbr_draw_inputs(view, object, light, frame, object_constants) ==
        granit::material::pbr_draw_input_error::invalid_light_direction);
  light.direction_to_light = {0.0F, 0.0F, 1.0F};
  light.radiance.x = -1.0F;
  CHECK(granit::material::pack_pbr_draw_inputs(view, object, light, frame, object_constants) ==
        granit::material::pbr_draw_input_error::negative_light_radiance);
  view.camera_position.x = std::numeric_limits<float>::infinity();
  light.radiance.x = 1.0F;
  CHECK(granit::material::pack_pbr_draw_inputs(view, object, light, frame, object_constants) ==
        granit::material::pbr_draw_input_error::non_finite_value);
}
