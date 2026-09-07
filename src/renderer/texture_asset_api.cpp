// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/texture_asset.h>

#include "assets/texture_asset.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace {

granit_result decode_manifest(const void* data, uint64_t size,
                              granit::detail::texture_asset_view& asset) {
  if (data == nullptr || size == 0 || size > std::numeric_limits<size_t>::max())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto decoded = granit::detail::decode_texture_asset(
      {static_cast<const std::byte*>(data), static_cast<size_t>(size)}, asset);
  if (decoded == granit::detail::texture_asset_error::unsupported_schema)
    return GRANIT_ERROR_UNSUPPORTED;
  return decoded == granit::detail::texture_asset_error::success ? GRANIT_SUCCESS
                                                                 : GRANIT_ERROR_INVALID_ARGUMENT;
}

} // namespace

extern "C" granit_result granit_texture_asset_inspect(const void* manifest_data,
                                                      uint64_t manifest_size,
                                                      granit_texture_asset_info* info) {
  if (info == nullptr || info->struct_size < GRANIT_TEXTURE_ASSET_INFO_SIZE ||
      info->reserved != 0 || info->reserved_2 != 0 ||
      (info->variants == nullptr && info->variant_capacity != 0) ||
      (info->subresources == nullptr && info->subresource_capacity != 0))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    granit::detail::texture_asset_view asset;
    const auto result = decode_manifest(manifest_data, manifest_size, asset);
    if (result != GRANIT_SUCCESS)
      return result;
    info->schema_version = GRANIT_TEXTURE_ASSET_SCHEMA_VERSION;
    std::memcpy(info->content_id, asset.content_id.data(), asset.content_id.size());
    info->dimension = asset.dimension;
    info->width = asset.width;
    info->height = asset.height;
    info->depth = asset.depth;
    info->array_layers = asset.array_layers;
    info->mip_levels = asset.mip_levels;
    info->variant_count = static_cast<uint32_t>(asset.variants.size());
    info->subresource_count = static_cast<uint32_t>(asset.subresources.size());
    if (info->variants == nullptr && info->subresources == nullptr)
      return GRANIT_SUCCESS;
    if (info->variants == nullptr || info->variant_capacity < asset.variants.size() ||
        info->subresources == nullptr || info->subresource_capacity < asset.subresources.size())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    std::ranges::copy(asset.variants, info->variants);
    std::ranges::copy(asset.subresources, info->subresources);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_renderer_select_texture_asset_variant(
    granit_renderer renderer, const void* manifest_data, uint64_t manifest_size,
    const granit_texture_asset_selection_desc* desc, granit_texture_asset_selection* selection) {
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_TEXTURE_ASSET_SELECTION_DESC_SIZE ||
      desc->required_usage == 0 || desc->reserved != 0 || selection == nullptr ||
      selection->struct_size < GRANIT_TEXTURE_ASSET_SELECTION_SIZE || selection->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  selection->variant_index = UINT32_MAX;
  selection->format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  selection->payload_offset = 0;
  selection->payload_size = 0;
  try {
    granit::detail::texture_asset_view asset;
    const auto result = decode_manifest(manifest_data, manifest_size, asset);
    if (result != GRANIT_SUCCESS)
      return result;
    for (uint32_t index = 0; index < asset.variants.size(); ++index) {
      const auto& variant = asset.variants[index];
      if ((desc->required_usage & ~variant.usage) != 0)
        continue;
      granit_texture_format_capabilities capabilities = GRANIT_TEXTURE_FORMAT_CAPABILITIES_INIT;
      const auto capability_result =
          granit_renderer_get_texture_format_capabilities(renderer, variant.format, &capabilities);
      if (capability_result != GRANIT_SUCCESS)
        return capability_result;
      if ((desc->required_usage & ~capabilities.supported_usage) != 0 ||
          (desc->required_features & ~capabilities.features) != 0)
        continue;
      selection->variant_index = index;
      selection->format = variant.format;
      selection->payload_offset = variant.payload_offset;
      selection->payload_size = variant.payload_size;
      return GRANIT_SUCCESS;
    }
    return GRANIT_ERROR_UNSUPPORTED;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_upload_batch_write_texture_asset_mips(
    granit_renderer renderer, granit_upload_batch batch, granit_texture texture,
    const void* manifest_data, uint64_t manifest_size, const void* payload_data,
    uint64_t payload_size, uint32_t variant_index, uint32_t first_mip, uint32_t mip_count) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE ||
      texture == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (payload_data == nullptr || payload_size == 0 ||
      payload_size > std::numeric_limits<size_t>::max() || mip_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    granit::detail::texture_asset_view asset;
    const auto result = decode_manifest(manifest_data, manifest_size, asset);
    if (result != GRANIT_SUCCESS)
      return result;
    if (variant_index >= asset.variants.size() || first_mip >= asset.mip_levels ||
        mip_count > asset.mip_levels - first_mip)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto payload =
        std::span{static_cast<const std::byte*>(payload_data), static_cast<size_t>(payload_size)};
    if (!granit::detail::validate_texture_asset_payload(asset, variant_index, payload))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto& variant = asset.variants[variant_index];
    uint64_t staged_bytes = 0;
    uint32_t operation_count = 0;
    for (uint32_t index = 0; index < variant.subresource_count; ++index) {
      const auto& source = asset.subresources[variant.first_subresource + index];
      if (source.mip_level < first_mip || source.mip_level >= first_mip + mip_count)
        continue;
      if (source.data_size > UINT64_MAX - staged_bytes)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      staged_bytes += source.data_size;
      ++operation_count;
    }
    granit_upload_batch_info batch_info = GRANIT_UPLOAD_BATCH_INFO_INIT;
    const auto info_result = granit_upload_batch_get_info(renderer, batch, &batch_info);
    if (info_result != GRANIT_SUCCESS)
      return info_result;
    if (batch_info.staged_bytes != 0 || batch_info.operation_count != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if ((batch_info.max_staged_bytes != 0 && staged_bytes > batch_info.max_staged_bytes) ||
        (batch_info.max_operation_count != 0 && operation_count > batch_info.max_operation_count))
      return GRANIT_ERROR_NOT_READY;
    for (uint32_t index = 0; index < variant.subresource_count; ++index) {
      const auto& source = asset.subresources[variant.first_subresource + index];
      if (source.mip_level < first_mip || source.mip_level >= first_mip + mip_count)
        continue;
      const auto shift = std::min(source.mip_level, UINT32_C(31));
      const auto width = std::max(UINT32_C(1), asset.width >> shift);
      const auto height = std::max(UINT32_C(1), asset.height >> shift);
      const auto depth = asset.dimension == GRANIT_TEXTURE_DIMENSION_3D
                             ? std::max(UINT32_C(1), asset.depth >> shift)
                             : UINT32_C(1);
      const granit_texture_data_layout layout{UINT64_C(0), source.bytes_per_row,
                                              source.rows_per_image};
      const granit_texture_write_region region{source.mip_level, source.array_layer,
                                               UINT32_C(1),      GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                               UINT32_C(0),      UINT32_C(0),
                                               UINT32_C(0),      width,
                                               height,           depth};
      const auto write_result = granit_upload_batch_write_texture(
          renderer, batch, texture,
          payload.data() + static_cast<size_t>(variant.payload_offset + source.data_offset),
          source.data_size, &layout, &region);
      if (write_result != GRANIT_SUCCESS) {
        static_cast<void>(granit_upload_batch_reset(renderer, batch));
        return write_result;
      }
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
