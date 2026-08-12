// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/ibl_resources.h"

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include <catch2/catch_all.hpp>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("IBL Group3资源绑定两个Cube和BRDF LUT") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-ibl-resources"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::texture irradiance_texture;
  granit::texture prefiltered_texture;
  granit::texture lut_texture;
  granit::texture_view irradiance_view;
  granit::texture_view prefiltered_view;
  granit::texture_view lut_view;
  const auto cube_desc = granit::texture_desc{.dimension = granit::texture_dimension::cube,
                                               .format = granit::texture_format::rgba16_float,
                                               .usage = granit::texture_usage::sampled,
                                               .width = 8,
                                               .height = 8,
                                               .mip_levels = 4,
                                               .array_layers = 6};
  REQUIRE(irradiance_texture.initialize(renderer.native_handle(), cube_desc) ==
          granit::result::success);
  REQUIRE(prefiltered_texture.initialize(renderer.native_handle(), cube_desc) ==
          granit::result::success);
  REQUIRE(lut_texture.initialize(renderer.native_handle(),
                                 {.format = granit::texture_format::rgba16_float,
                                  .usage = granit::texture_usage::sampled,
                                  .width = 8,
                                  .height = 8}) == granit::result::success);
  const auto cube_view_desc = granit::texture_view_desc{
      .dimension = granit::texture_dimension::cube,
      .mip_level_count = 4,
      .array_layer_count = 6};
  REQUIRE(irradiance_view.initialize(renderer.native_handle(), irradiance_texture.native_handle(),
                                     cube_view_desc) ==
          granit::result::success);
  REQUIRE(prefiltered_view.initialize(renderer.native_handle(),
                                     prefiltered_texture.native_handle(), cube_view_desc) ==
          granit::result::success);
  REQUIRE(lut_view.initialize(renderer.native_handle(), lut_texture.native_handle()) ==
          granit::result::success);

  granit::lighting::ibl_resources resources;
  granit::lighting::ibl_sampling_constants constants{.rotation_cos = 0.0F,
                                                      .rotation_sin = 1.0F,
                                                      .intensity = 2.0F,
                                                      .prefiltered_max_mip = 3.0F};
  REQUIRE(resources.initialize(renderer.native_handle(),
                               {.irradiance = irradiance_view.native_handle(),
                                .prefiltered_environment = prefiltered_view.native_handle(),
                                .brdf_lut = lut_view.native_handle()},
                               constants) == GRANIT_SUCCESS);
  CHECK(resources.layout() != GRANIT_NULL_HANDLE);
  CHECK(resources.group() != GRANIT_NULL_HANDLE);

  constants.intensity = 0.5F;
  CHECK(resources.update(constants) == GRANIT_SUCCESS);
  constants.prefiltered_max_mip = -1.0F;
  CHECK(resources.update(constants) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("IBL Group3资源拒绝缺失纹理") {
  granit::lighting::ibl_resources resources;
  const granit::lighting::ibl_sampling_constants constants{};
  CHECK(resources.initialize(GRANIT_NULL_HANDLE, {}, constants) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(resources.update(constants) == GRANIT_ERROR_INVALID_ARGUMENT);
}
