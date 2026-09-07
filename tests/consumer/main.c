// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>

#include "linkage_check.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  char header_version[32] = {0};
  (void)snprintf(header_version, sizeof(header_version), "%u.%u.%u", GRANIT_VERSION_MAJOR,
                 GRANIT_VERSION_MINOR, GRANIT_VERSION_PATCH);
  if (strcmp(header_version, GRANIT_CONSUMER_PACKAGE_VERSION) != 0)
    return 1;
  if (granit_version_major() != GRANIT_VERSION_MAJOR ||
      granit_version_minor() != GRANIT_VERSION_MINOR ||
      granit_version_patch() != GRANIT_VERSION_PATCH)
    return 2;

  granit_texture_asset_variant_info variant = {0};
  variant.format = GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  variant.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  variant.subresource_count = 1;
  variant.payload_size = 64;
  granit_texture_asset_subresource_info subresource = {0};
  subresource.data_size = 64;
  subresource.bytes_per_row = 16;
  subresource.rows_per_image = 4;
  granit_texture_asset_info asset = GRANIT_TEXTURE_ASSET_INFO_INIT;
  asset.schema_version = GRANIT_TEXTURE_ASSET_SCHEMA_VERSION;
  asset.content_id[0] = 1;
  asset.dimension = GRANIT_TEXTURE_DIMENSION_2D;
  asset.width = 4;
  asset.height = 4;
  asset.depth = 1;
  asset.array_layers = 1;
  asset.mip_levels = 1;
  asset.variant_count = 1;
  asset.subresource_count = 1;
  asset.variants = &variant;
  asset.variant_capacity = 1;
  asset.subresources = &subresource;
  asset.subresource_capacity = 1;
  uint64_t manifest_size = 0;
  if (granit_texture_asset_encode(&asset, NULL, &manifest_size) != GRANIT_SUCCESS ||
      manifest_size != UINT64_C(192))
    return 14;
  unsigned char manifest[192] = {0};
  if (granit_texture_asset_encode(&asset, manifest, &manifest_size) != GRANIT_SUCCESS)
    return 15;
  granit_texture_asset_info inspected = GRANIT_TEXTURE_ASSET_INFO_INIT;
  if (granit_texture_asset_inspect(manifest, manifest_size, &inspected) != GRANIT_SUCCESS ||
      inspected.variant_count != 1 || inspected.subresource_count != 1)
    return 16;

  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_renderer_desc invalid_desc = GRANIT_RENDERER_DESC_INIT;
  invalid_desc.struct_size = 0;
  if (granit_renderer_create(&invalid_desc, &renderer) != GRANIT_ERROR_INVALID_ARGUMENT ||
      renderer != GRANIT_NULL_HANDLE)
    return 3;

  const granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  const granit_result renderer_result = granit_renderer_create(&renderer_desc, &renderer);
  if (renderer_result == GRANIT_ERROR_BACKEND_UNAVAILABLE ||
      renderer_result == GRANIT_ERROR_INCOMPATIBLE_DRIVER ||
      renderer_result == GRANIT_ERROR_NO_SUITABLE_DEVICE)
    return renderer == GRANIT_NULL_HANDLE ? 0 : 4;
  if (renderer_result != GRANIT_SUCCESS || renderer == GRANIT_NULL_HANDLE)
    return 5;

  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  if (granit_renderer_get_limits(renderer, &limits) != GRANIT_SUCCESS ||
      limits.uniform_buffer_offset_alignment == 0 || limits.max_uniform_buffer_binding_size == 0)
    return 6;
  granit_renderer_shader_capabilities shader_capabilities =
      GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
  if (granit_renderer_get_shader_capabilities(renderer, &shader_capabilities) != GRANIT_SUCCESS ||
      shader_capabilities.backend != GRANIT_RENDERER_BACKEND_VULKAN ||
      shader_capabilities.profile != GRANIT_SHADER_PROFILE_PORTABLE)
    return 12;
  granit_shader_variant_requirement shader_variant = GRANIT_SHADER_VARIANT_REQUIREMENT_INIT;
  shader_variant.backend = GRANIT_RENDERER_BACKEND_VULKAN;
  uint32_t selected_variant = UINT32_MAX;
  if (granit_renderer_select_shader_variant(renderer, &shader_variant, 1, &selected_variant) !=
          GRANIT_SUCCESS ||
      selected_variant != 0)
    return 13;

  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT;
  buffer_desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
  buffer_desc.size = UINT64_C(64);
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  if (granit_buffer_create(renderer, &buffer_desc, &buffer) != GRANIT_SUCCESS ||
      buffer == GRANIT_NULL_HANDLE)
    return 7;
  if (granit_buffer_destroy(renderer, buffer) != GRANIT_SUCCESS)
    return 8;
  if (granit_buffer_destroy(renderer, buffer) != GRANIT_ERROR_INVALID_HANDLE)
    return 9;
  if (granit_renderer_destroy(renderer) != GRANIT_SUCCESS)
    return 10;
  return granit_renderer_destroy(renderer) == GRANIT_ERROR_INVALID_HANDLE ? 0 : 11;
}
