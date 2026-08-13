// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_reference.h"

#include <algorithm>
#include <cmath>

namespace granit::lighting {
namespace {

float saturate(float value) noexcept { return std::clamp(value, 0.0F, 1.0F); }

bool is_srgb_format(granit::texture_format format) noexcept {
  return format == granit::texture_format::rgba8_srgb ||
         format == granit::texture_format::bgra8_srgb;
}

bool is_unorm_format(granit::texture_format format) noexcept {
  return format == granit::texture_format::rgba8_unorm ||
         format == granit::texture_format::bgra8_unorm;
}

} // namespace

tone_mapping_error validate_tone_mapping_output(granit::texture_format format,
                                                tone_mapping_output_transfer transfer) noexcept {
  if (transfer == tone_mapping_output_transfer::attachment_srgb && is_srgb_format(format))
    return tone_mapping_error::none;
  if (transfer == tone_mapping_output_transfer::shader_srgb && is_unorm_format(format))
    return tone_mapping_error::none;
  return tone_mapping_error::incompatible_output;
}

float aces_fitted(float value) noexcept {
  const auto color = std::max(value, 0.0F);
  const auto numerator = color * (2.51F * color + 0.03F);
  const auto denominator = color * (2.43F * color + 0.59F) + 0.14F;
  return denominator > 0.0F ? saturate(numerator / denominator) : 0.0F;
}

float linear_to_srgb(float value) noexcept {
  const auto color = saturate(value);
  if (color <= 0.0F)
    return 0.0F;
  if (color >= 1.0F)
    return 1.0F;
  if (color <= 0.0031308F)
    return 12.92F * color;
  return 1.055F * std::pow(color, 1.0F / 2.4F) - 0.055F;
}

tone_mapping_error evaluate_tone_mapping(math::float3 hdr_color, const tone_mapping_desc& desc,
                                         math::float3& output) noexcept {
  if (!math::is_finite(hdr_color))
    return tone_mapping_error::invalid_color;
  if (!std::isfinite(desc.exposure_ev) || desc.exposure_ev < tone_mapping_min_exposure_ev ||
      desc.exposure_ev > tone_mapping_max_exposure_ev)
    return tone_mapping_error::invalid_exposure;

  const auto exposure = std::exp2(desc.exposure_ev);
  auto candidate = math::float3{aces_fitted(hdr_color.x * exposure),
                                aces_fitted(hdr_color.y * exposure),
                                aces_fitted(hdr_color.z * exposure)};
  if (desc.output_transfer == tone_mapping_output_transfer::shader_srgb) {
    candidate = {linear_to_srgb(candidate.x), linear_to_srgb(candidate.y),
                 linear_to_srgb(candidate.z)};
  }
  output = candidate;
  return tone_mapping_error::none;
}

} // namespace granit::lighting
