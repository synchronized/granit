// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/texture_builder.h>

#include <stdint.h>
#include <string.h>

int main(void) {
  granit_texture_asset_subresource_info subresource = {0};
  granit_asset_tools_texture_variant_desc variant = GRANIT_ASSET_TOOLS_TEXTURE_VARIANT_DESC_INIT;
  granit_asset_tools_texture_build_desc desc = GRANIT_ASSET_TOOLS_TEXTURE_BUILD_DESC_INIT;
  granit_asset_tools_texture_result result = 0;
  unsigned char bytes[64] = {0};
  const void* output = NULL;
  uint64_t output_size = 0;
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
      granit_asset_tools_texture_result_get_manifest(result, &output, &output_size) !=
          GRANIT_SUCCESS ||
      output == NULL || output_size != 192 ||
      granit_asset_tools_texture_result_get_payload(result, &output, &output_size) !=
          GRANIT_SUCCESS ||
      output == NULL || output_size != sizeof(bytes) || memcmp(output, bytes, sizeof(bytes)) != 0 ||
      granit_asset_tools_texture_result_destroy(result) != GRANIT_SUCCESS ||
      granit_asset_tools_texture_result_destroy(result) != GRANIT_ERROR_INVALID_HANDLE)
    return 1;
  output = (const void*)1;
  output_size = 1;
  if (granit_asset_tools_texture_result_get_manifest(0, &output, &output_size) !=
          GRANIT_ERROR_INVALID_HANDLE ||
      output != NULL || output_size != 0)
    return 2;
  return 0;
}
