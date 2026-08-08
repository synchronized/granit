// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/resource_validation.h"

#include <cmath>

namespace granit::detail {
namespace {

constexpr granit_buffer_usage buffer_usage_mask =
    GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT | GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT |
    GRANIT_BUFFER_USAGE_VERTEX_BIT | GRANIT_BUFFER_USAGE_INDEX_BIT |
    GRANIT_BUFFER_USAGE_UNIFORM_BIT | GRANIT_BUFFER_USAGE_STORAGE_BIT |
    GRANIT_BUFFER_USAGE_INDIRECT_BIT;
constexpr granit_texture_usage texture_usage_mask =
    GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT |
    GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_STORAGE_BIT |
    GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
constexpr granit_texture_aspect texture_aspect_mask = GRANIT_TEXTURE_ASPECT_COLOR_BIT |
                                                      GRANIT_TEXTURE_ASPECT_DEPTH_BIT |
                                                      GRANIT_TEXTURE_ASPECT_STENCIL_BIT;

bool valid_memory_location(granit_memory_location location) noexcept {
  return location <= GRANIT_MEMORY_LOCATION_READBACK;
}

bool valid_texture_format(granit_texture_format format) noexcept {
  return format >= GRANIT_TEXTURE_FORMAT_R8_UNORM &&
         format <= GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
}

bool valid_texture_dimension(granit_texture_dimension dimension) noexcept {
  return dimension >= GRANIT_TEXTURE_DIMENSION_1D && dimension <= GRANIT_TEXTURE_DIMENSION_CUBE;
}

bool valid_sample_count(granit_sample_count sample_count) noexcept {
  return sample_count == GRANIT_SAMPLE_COUNT_1 || sample_count == GRANIT_SAMPLE_COUNT_2 ||
         sample_count == GRANIT_SAMPLE_COUNT_4 || sample_count == GRANIT_SAMPLE_COUNT_8;
}

bool depth_stencil_format(granit_texture_format format) noexcept {
  return format >= GRANIT_TEXTURE_FORMAT_D16_UNORM;
}

} // namespace

granit_result validate_buffer_desc(const granit_buffer_desc& desc) noexcept {
  if (desc.struct_size < GRANIT_BUFFER_DESC_VERSION_1_SIZE || desc.reserved != 0 ||
      desc.reserved_2 != 0 || desc.size == 0 || desc.usage == 0 ||
      (desc.usage & ~buffer_usage_mask) != 0 || !valid_memory_location(desc.memory_location)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

granit_result validate_texture_desc(const granit_texture_desc& desc) noexcept {
  if (desc.struct_size < GRANIT_TEXTURE_DESC_VERSION_1_SIZE || desc.reserved != 0 ||
      !valid_texture_dimension(desc.dimension) || !valid_texture_format(desc.format) ||
      !valid_sample_count(desc.sample_count) || desc.usage == 0 ||
      (desc.usage & ~texture_usage_mask) != 0 || !valid_memory_location(desc.memory_location) ||
      desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.mip_levels == 0 ||
      desc.array_layers == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if ((depth_stencil_format(desc.format) &&
       (desc.usage & GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) != 0) ||
      (!depth_stencil_format(desc.format) &&
       (desc.usage & GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.dimension != GRANIT_TEXTURE_DIMENSION_2D || desc.depth != 1 || desc.mip_levels != 1 ||
      desc.array_layers != 1 || desc.sample_count != GRANIT_SAMPLE_COUNT_1 ||
      desc.memory_location == GRANIT_MEMORY_LOCATION_UPLOAD ||
      desc.memory_location == GRANIT_MEMORY_LOCATION_READBACK) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return GRANIT_SUCCESS;
}

granit_result validate_texture_view_desc(const granit_texture_view_desc& desc) noexcept {
  const auto& range = desc.range;
  if (desc.struct_size < GRANIT_TEXTURE_VIEW_DESC_VERSION_1_SIZE || desc.reserved != 0 ||
      desc.reserved_2 != 0 || desc.dimension < GRANIT_TEXTURE_DIMENSION_1D ||
      desc.dimension > GRANIT_TEXTURE_DIMENSION_CUBE ||
      (desc.format != GRANIT_TEXTURE_FORMAT_UNDEFINED && !valid_texture_format(desc.format)) ||
      (range.aspect & ~texture_aspect_mask) != 0 || range.mip_level_count == 0 ||
      range.array_layer_count == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.dimension != GRANIT_TEXTURE_DIMENSION_2D || range.base_mip_level != 0 ||
      range.mip_level_count != 1 || range.base_array_layer != 0 || range.array_layer_count != 1) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return GRANIT_SUCCESS;
}

granit_result validate_sampler_desc(const granit_sampler_desc& desc) noexcept {
  if (desc.struct_size < GRANIT_SAMPLER_DESC_VERSION_1_SIZE || desc.reserved != 0 ||
      desc.mag_filter > GRANIT_FILTER_LINEAR || desc.min_filter > GRANIT_FILTER_LINEAR ||
      desc.mipmap_filter > GRANIT_MIPMAP_FILTER_LINEAR ||
      desc.address_mode_u > GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE ||
      desc.address_mode_v > GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE ||
      desc.address_mode_w > GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE ||
      desc.compare_operation > GRANIT_COMPARE_OPERATION_ALWAYS || desc.anisotropy_enabled > 1 ||
      !std::isfinite(desc.max_anisotropy) || !std::isfinite(desc.lod_bias) ||
      !std::isfinite(desc.min_lod) || !std::isfinite(desc.max_lod) || desc.min_lod < 0.0F ||
      desc.max_lod < desc.min_lod) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if ((desc.anisotropy_enabled == 0 && desc.max_anisotropy != 1.0F) ||
      (desc.anisotropy_enabled != 0 && desc.max_anisotropy < 1.0F)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
