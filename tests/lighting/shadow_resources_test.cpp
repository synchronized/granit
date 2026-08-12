// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/shadow_resources.h"

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

TEST_CASE("阴影Group3资源拥有常量和比较Sampler") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-shadow-resources"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::texture texture;
  granit::texture_view view;
  REQUIRE(texture.initialize(renderer.native_handle(),
                             {.format = granit::texture_format::d32_float,
                              .usage = granit::texture_usage::depth_stencil_attachment |
                                       granit::texture_usage::sampled,
                              .width = 64,
                              .height = 64}) == granit::result::success);
  REQUIRE(view.initialize(renderer.native_handle(), texture.native_handle()) ==
          granit::result::success);

  granit::lighting::shadow_sampling_constants constants{.light_view_projection =
                                                            granit::math::identity_matrix4,
                                                        .depth_bias = 0.001F,
                                                        .normal_bias = 0.01F,
                                                        .texel_size = {1.0F / 64.0F, 1.0F / 64.0F}};
  granit::lighting::shadow_resources resources;
  REQUIRE(resources.initialize(renderer.native_handle(), view.native_handle(), constants) ==
          GRANIT_SUCCESS);
  CHECK(resources.layout() != GRANIT_NULL_HANDLE);
  CHECK(resources.group() != GRANIT_NULL_HANDLE);

  constants.depth_bias = 0.002F;
  CHECK(resources.update(constants) == GRANIT_SUCCESS);
  constants.texel_size[0] = 0.0F;
  CHECK(resources.update(constants) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("阴影Group3资源拒绝不完整描述") {
  granit::lighting::shadow_resources resources;
  granit::lighting::shadow_sampling_constants constants{};
  CHECK(resources.initialize(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, constants) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(resources.update(constants) == GRANIT_ERROR_INVALID_ARGUMENT);
}
