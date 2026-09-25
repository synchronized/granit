// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/texture_builder.h>

#include <stdint.h>
#include <string.h>

int main(void) {
  granit_texture_asset_subresource_info subresource = {0};
  granit_asset_tools_texture_variant_desc variant = GRANIT_ASSET_TOOLS_TEXTURE_VARIANT_DESC_INIT;
  granit_asset_tools_texture_build_desc desc = GRANIT_ASSET_TOOLS_TEXTURE_BUILD_DESC_INIT;
  granit_asset_tools_texture_result result = 0;
  granit_asset_tools_texture_result_info info = GRANIT_ASSET_TOOLS_TEXTURE_RESULT_INFO_INIT;
  unsigned char bytes[64] = {0};
  subresource.data_size = 64;
  subresource.bytes_per_row = 16;
  subresource.rows_per_image = 4;
  variant.format = GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  variant.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
  variant.payload = bytes;
  variant.payload_size = sizeof(bytes);
  variant.subresources = &subresource;
  variant.subresource_count = 1;
  desc.width = 4;
  desc.height = 4;
  desc.variants = &variant;
  desc.variant_count = 1;
  if (granit_asset_tools_texture_build(&desc, &result) != GRANIT_SUCCESS || result == 0 ||
      granit_asset_tools_texture_result_get_info(result, &info) != GRANIT_SUCCESS ||
      info.manifest == NULL || info.manifest_size != 192 || info.payload == NULL ||
      info.payload_size != sizeof(bytes) || memcmp(info.payload, bytes, sizeof(bytes)) != 0 ||
      info.debug_json == NULL || info.debug_json_length == 0 || info.diagnostic_length != 0 ||
      granit_asset_tools_texture_result_destroy(result) != GRANIT_SUCCESS ||
      granit_asset_tools_texture_result_destroy(result) != GRANIT_ERROR_INVALID_HANDLE)
    return 1;
  info = (granit_asset_tools_texture_result_info)GRANIT_ASSET_TOOLS_TEXTURE_RESULT_INFO_INIT;
  info.reserved = 1;
  if (granit_asset_tools_texture_result_get_info(0, &info) != GRANIT_ERROR_INVALID_ARGUMENT)
    return 2;
  info = (granit_asset_tools_texture_result_info)GRANIT_ASSET_TOOLS_TEXTURE_RESULT_INFO_INIT;
  if (granit_asset_tools_texture_result_get_info(0, &info) != GRANIT_ERROR_INVALID_HANDLE ||
      info.manifest != NULL || info.manifest_size != 0)
    return 3;
  return 0;
}
