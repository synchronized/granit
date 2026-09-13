// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/environment_builder.h>

#include <stdint.h>

int main(void) {
  uint8_t irradiance[48] = {0};
  uint8_t prefiltered[48] = {0};
  uint8_t brdf[8] = {0};
  granit_asset_tools_environment_mip_desc mip = GRANIT_ASSET_TOOLS_ENVIRONMENT_MIP_DESC_INIT;
  granit_asset_tools_environment_build_desc desc = GRANIT_ASSET_TOOLS_ENVIRONMENT_BUILD_DESC_INIT;
  granit_asset_tools_environment_result result = 0;
  const void* package = 0;
  uint64_t package_size = 0;
  mip.resolution = 1;
  mip.pixels = prefiltered;
  mip.pixels_size = sizeof(prefiltered);
  desc.irradiance_resolution = 1;
  desc.irradiance_pixels = irradiance;
  desc.irradiance_pixels_size = sizeof(irradiance);
  desc.prefiltered_mips = &mip;
  desc.prefiltered_mip_count = 1;
  desc.brdf_width = 1;
  desc.brdf_height = 1;
  desc.brdf_pixels = brdf;
  desc.brdf_pixels_size = sizeof(brdf);
  if (granit_asset_tools_environment_build(&desc, &result) != GRANIT_SUCCESS || result == 0 ||
      granit_asset_tools_environment_result_get_package(result, &package, &package_size) !=
          GRANIT_SUCCESS ||
      package == 0 || package_size != 200)
    return 1;
  if (granit_asset_tools_environment_result_destroy(result) != GRANIT_SUCCESS)
    return 2;
  return granit_asset_tools_environment_result_destroy(result) == GRANIT_ERROR_INVALID_HANDLE ? 0
                                                                                              : 3;
}
