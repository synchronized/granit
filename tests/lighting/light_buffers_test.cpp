// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/light_buffers.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("逐View光源Buffer支持空集合和容量内更新") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-light-buffers"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::lighting::light_buffers buffers;
  REQUIRE(buffers.initialize(renderer.native_handle(), {.directional = 1, .point = 2, .spot = 1}) ==
          GRANIT_SUCCESS);
  CHECK(buffers.counts() != GRANIT_NULL_HANDLE);
  CHECK(buffers.directional() != GRANIT_NULL_HANDLE);
  CHECK(buffers.point() != GRANIT_NULL_HANDLE);
  CHECK(buffers.spot() != GRANIT_NULL_HANDLE);
  CHECK(buffers.update({}) == GRANIT_SUCCESS);

  granit::lighting::packed_view_lights lights;
  lights.directional.push_back({});
  lights.point.resize(2);
  lights.spot.push_back({});
  CHECK(buffers.update(lights) == GRANIT_SUCCESS);
}

TEST_CASE("逐View光源Buffer拒绝非法容量和溢出更新") {
  granit::lighting::light_buffers buffers;
  CHECK(buffers.initialize(GRANIT_NULL_HANDLE, {}) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(buffers.update({}) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-light-limits"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  CHECK(buffers.initialize(renderer.native_handle(),
                           {.directional = granit::lighting::maximum_directional_lights + 1}) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(buffers.initialize(renderer.native_handle(), {.directional = 0, .point = 1, .spot = 0}) ==
          GRANIT_SUCCESS);
  granit::lighting::packed_view_lights lights;
  lights.point.resize(2);
  CHECK(buffers.update(lights) == GRANIT_ERROR_INVALID_ARGUMENT);
}
