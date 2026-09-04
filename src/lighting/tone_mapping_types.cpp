// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_types.h"

namespace granit::lighting {

tone_mapping_error validate_tone_mapping_output(granit::texture_format format,
                                                tone_mapping_output_transfer transfer) noexcept {
  const auto srgb =
      format == granit::texture_format::rgba8_srgb || format == granit::texture_format::bgra8_srgb;
  const auto unorm = format == granit::texture_format::rgba8_unorm ||
                     format == granit::texture_format::bgra8_unorm;
  if ((transfer == tone_mapping_output_transfer::attachment_srgb && srgb) ||
      (transfer == tone_mapping_output_transfer::shader_srgb && unorm)) {
    return tone_mapping_error::none;
  }
  return tone_mapping_error::incompatible_output;
}

} // namespace granit::lighting
