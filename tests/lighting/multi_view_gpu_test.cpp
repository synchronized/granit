// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/light_data.h"
#include "lighting/shadow_ibl_resources.h"

#include <granit/granit.hpp>

#include <catch2/catch_all.hpp>

#include <array>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

granit::scene::view_input make_view(std::uint64_t layer_mask) {
  granit::scene::view_input view{};
  view.view = granit::math::identity_matrix4;
  view.projection = granit::math::identity_matrix4;
  view.view_projection = granit::math::identity_matrix4;
  view.layer_mask = layer_mask;
  return view;
}

} // namespace

TEST_CASE("两个View使用独立光源Buffer和渲染目标") {
  const std::array views{make_view(1), make_view(2)};
  const std::array points{
      granit::scene::point_light_input{
          .position = {0, 0, 0.5F}, .intensity = {1, 0, 0}, .radius = 2, .layer_mask = 1},
      granit::scene::point_light_input{
          .position = {0, 0, 0.5F}, .intensity = {0, 1, 0}, .radius = 2, .layer_mask = 2}};
  granit::scene::multi_view_snapshot snapshot;
  REQUIRE(granit::scene::build_multi_view_snapshot({.views = views,
                                                    .renderables = {},
                                                    .directional_lights = {},
                                                    .point_lights = points,
                                                    .spot_lights = {}},
                                                   snapshot) ==
          granit::scene::multi_view_error::none);

  std::array<granit::lighting::packed_view_lights, 2> packed;
  for (std::size_t index = 0; index < packed.size(); ++index) {
    granit::lighting::light_requirements requirements;
    REQUIRE(granit::lighting::pack_view_lights(
                snapshot, index, {.directional = 1, .point = 2, .spot = 1}, packed[index],
                requirements) == granit::lighting::light_pack_error::none);
    REQUIRE(packed[index].point.size() == 1);
  }
  CHECK(packed[0].point[0].intensity[0] == 1.0F);
  CHECK(packed[1].point[0].intensity[1] == 1.0F);

  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-multi-view-gpu"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  std::array<granit::lighting::shadow_ibl_resources, 2> light_resources;
  for (std::size_t index = 0; index < light_resources.size(); ++index) {
    REQUIRE(light_resources[index].initialize(renderer.native_handle(), {}, {}, {},
                                              {.directional = 1, .point = 2, .spot = 1},
                                              {.shadows = false, .ibl = false}) == GRANIT_SUCCESS);
    REQUIRE(light_resources[index].update_lights(packed[index]) == GRANIT_SUCCESS);
  }
  CHECK(light_resources[0].layout() != light_resources[1].layout());
  CHECK(light_resources[0].group() != light_resources[1].group());

  std::array<granit::texture, 2> colors;
  std::array<granit::texture_view, 2> color_views;
  std::array<granit::texture, 2> depths;
  std::array<granit::texture_view, 2> depth_views;
  for (std::size_t index = 0; index < colors.size(); ++index) {
    REQUIRE(colors[index].initialize(renderer.native_handle(),
                                     {.format = granit::texture_format::rgba16_float,
                                      .usage = granit::texture_usage::color_attachment,
                                      .width = 32,
                                      .height = 32}) == granit::result::success);
    REQUIRE(
        color_views[index].initialize(renderer.native_handle(), colors[index].native_handle()) ==
        granit::result::success);
    REQUIRE(depths[index].initialize(renderer.native_handle(),
                                     {.format = granit::texture_format::d32_float,
                                      .usage = granit::texture_usage::depth_stencil_attachment,
                                      .width = 32,
                                      .height = 32}) == granit::result::success);
    REQUIRE(
        depth_views[index].initialize(renderer.native_handle(), depths[index].native_handle()) ==
        granit::result::success);
  }

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  for (std::size_t index = 0; index < colors.size(); ++index) {
    const granit::color_attachment_desc color{
        .view = color_views[index].native_handle(),
        .clear_value = {.red = static_cast<float>(index), .alpha = 1.0F}};
    const granit::depth_stencil_attachment_desc depth{.view = depth_views[index].native_handle(),
                                                      .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                           .depth_stencil_attachment = &depth,
                                           .area = {0, 0, 32, 32}};
    REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
    REQUIRE(recorder.end_rendering() == granit::result::success);
  }
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
}
