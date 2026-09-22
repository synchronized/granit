// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/debug_draw_list.hpp>
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
  REQUIRE(environment.initialize_builtin(renderer).ok());
  CHECK(environment.valid());

  granit::environment_map_info info;
  REQUIRE(environment.get_info(info).ok());
  CHECK(info.environment.irradiance.valid());
  CHECK(info.environment.prefiltered_environment.valid());
  CHECK(info.environment.brdf_lut.valid());
  CHECK(info.environment.intensity > 0.0F);

  granit::debug_draw_list debug;
  REQUIRE(debug.initialize(renderer.native_handle(), GRANIT_DEBUG_DRAW_LIST_DESC_INIT).ok());
  CHECK(environment.native_handle() != debug.native_handle());
  granit_environment_map_info cross_info = GRANIT_ENVIRONMENT_MAP_INFO_INIT;
  granit_debug_draw_list_stats debug_stats = GRANIT_DEBUG_DRAW_LIST_STATS_INIT;
  CHECK(granit_environment_map_get_info(renderer.native_handle(), debug.native_handle(),
                                        &cross_info) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_debug_draw_list_get_stats(renderer.native_handle(), environment.native_handle(),
                                         &debug_stats) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(debug.destroy().ok());

  granit::renderer_resource_stats live;
  REQUIRE(renderer.get_resource_stats(live).ok());
  CHECK(live.texture_count == before.texture_count + 3);
  CHECK(live.texture_view_count == before.texture_view_count + 3);

  const auto stale = environment.native_handle();
  REQUIRE(environment.reset().ok());
  REQUIRE(renderer.get_resource_stats(live).ok());
  CHECK(live.texture_count == before.texture_count);
  CHECK(live.texture_view_count == before.texture_view_count);
  CHECK(granit_environment_map_get_info(renderer.native_handle(), stale, &cross_info) ==
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
