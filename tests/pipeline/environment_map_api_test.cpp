// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/environment_map.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("Environment Map 拥有并释放 IBL 纹理", "[pipeline][environment-map]") {
  granit::renderer renderer;
  const granit::renderer_desc renderer_desc;
  const auto initialized = renderer.initialize(renderer_desc);
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized.ok());

  granit::renderer_resource_stats before;
  REQUIRE(renderer.get_resource_stats(before).ok());
  granit::environment_map environment;
  REQUIRE(environment.initialize_builtin(renderer.native_handle()).ok());
  CHECK(environment.valid());

  granit_environment_map_info info = GRANIT_ENVIRONMENT_MAP_INFO_INIT;
  REQUIRE(environment.get_info(info).ok());
  CHECK(info.environment.irradiance != GRANIT_NULL_HANDLE);
  CHECK(info.environment.prefiltered_environment != GRANIT_NULL_HANDLE);
  CHECK(info.environment.brdf_lut != GRANIT_NULL_HANDLE);
  CHECK(info.environment.intensity > 0.0F);

  granit::renderer_resource_stats live;
  REQUIRE(renderer.get_resource_stats(live).ok());
  CHECK(live.texture_count == before.texture_count + 3);
  CHECK(live.texture_view_count == before.texture_view_count + 3);

  const auto stale = environment.native_handle();
  REQUIRE(environment.reset().ok());
  REQUIRE(renderer.get_resource_stats(live).ok());
  CHECK(live.texture_count == before.texture_count);
  CHECK(live.texture_view_count == before.texture_view_count);
  CHECK(granit_environment_map_get_info(renderer.native_handle(), stale, &info) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_environment_map_destroy(renderer.native_handle(), stale) ==
        GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Environment Map 拒绝非法描述", "[pipeline][environment-map]") {
  granit_environment_map output = GRANIT_NULL_HANDLE;
  granit_environment_map_asset_desc desc = GRANIT_ENVIRONMENT_MAP_ASSET_DESC_INIT;
  CHECK(granit_environment_map_create_from_asset(GRANIT_NULL_HANDLE, nullptr, &output) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_environment_map_create_from_asset(GRANIT_NULL_HANDLE, &desc, nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_environment_map_get_info(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}
