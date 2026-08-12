// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/shadow_ibl_resources.h"

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

TEST_CASE("阴影和IBL共享完整Group3") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-shadow-ibl"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::texture shadow_texture;
  granit::texture irradiance_texture;
  granit::texture prefiltered_texture;
  granit::texture lut_texture;
  granit::texture_view shadow_view;
  granit::texture_view irradiance_view;
  granit::texture_view prefiltered_view;
  granit::texture_view lut_view;
  REQUIRE(shadow_texture.initialize(renderer.native_handle(),
                                    {.format = granit::texture_format::d32_float,
                                     .usage = granit::texture_usage::sampled |
                                              granit::texture_usage::depth_stencil_attachment}) ==
          granit::result::success);
  const auto cube_desc = granit::texture_desc{.dimension = granit::texture_dimension::cube,
                                               .format = granit::texture_format::rgba16_float,
                                               .usage = granit::texture_usage::sampled,
                                               .width = 4,
                                               .height = 4,
                                               .mip_levels = 3,
                                               .array_layers = 6};
  REQUIRE(irradiance_texture.initialize(renderer.native_handle(), cube_desc) ==
          granit::result::success);
  REQUIRE(prefiltered_texture.initialize(renderer.native_handle(), cube_desc) ==
          granit::result::success);
  REQUIRE(lut_texture.initialize(renderer.native_handle(),
                                 {.format = granit::texture_format::rgba16_float,
                                  .usage = granit::texture_usage::sampled}) ==
          granit::result::success);
  REQUIRE(shadow_view.initialize(renderer.native_handle(), shadow_texture.native_handle()) ==
          granit::result::success);
  const granit::texture_view_desc cube_view{.dimension = granit::texture_dimension::cube,
                                            .mip_level_count = 3,
                                            .array_layer_count = 6};
  REQUIRE(irradiance_view.initialize(renderer.native_handle(), irradiance_texture.native_handle(),
                                     cube_view) == granit::result::success);
  REQUIRE(prefiltered_view.initialize(renderer.native_handle(),
                                      prefiltered_texture.native_handle(), cube_view) ==
          granit::result::success);
  REQUIRE(lut_view.initialize(renderer.native_handle(), lut_texture.native_handle()) ==
          granit::result::success);

  granit::lighting::shadow_ibl_resources resources;
  REQUIRE(resources.initialize(
              renderer.native_handle(),
              {.shadow = shadow_view.native_handle(),
               .ibl = {.irradiance = irradiance_view.native_handle(),
                       .prefiltered_environment = prefiltered_view.native_handle(),
                       .brdf_lut = lut_view.native_handle()}},
              {.light_view_projection = granit::math::identity_matrix4,
               .depth_bias = 0.0F,
               .normal_bias = 0.0F,
               .texel_size = {1.0F, 1.0F}},
              {.prefiltered_max_mip = 2.0F}) == GRANIT_SUCCESS);
  CHECK(resources.layout() != GRANIT_NULL_HANDLE);
  CHECK(resources.group() != GRANIT_NULL_HANDLE);
  CHECK(resources.update_ibl({.intensity = 2.0F, .prefiltered_max_mip = 2.0F}) ==
        GRANIT_SUCCESS);
}

TEST_CASE("统一Group3拒绝不完整资源") {
  granit::lighting::shadow_ibl_resources resources;
  CHECK(resources.initialize(GRANIT_NULL_HANDLE, {}, {}, {}) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(resources.update_shadow({}) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(resources.update_ibl({}) == GRANIT_ERROR_INVALID_ARGUMENT);
}
