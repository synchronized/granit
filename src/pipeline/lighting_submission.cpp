// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/lighting_submission.h"

#include <cmath>

namespace granit::pipeline::detail {

granit_result build_lighting_submission(const scene::multi_view_snapshot& snapshot,
                                        std::uint32_t view_index,
                                        const lighting::light_limits& limits,
                                        lighting::ibl_texture_views default_ibl,
                                        const granit_render_pipeline_environment* environment,
                                        lighting_submission& output) noexcept {
  lighting_submission result;
  result.ibl_views = default_ibl;
  if (environment != nullptr) {
    result.ibl_views = {.irradiance = environment->irradiance,
                        .prefiltered_environment = environment->prefiltered_environment,
                        .brdf_lut = environment->brdf_lut};
    result.ibl_constants = {.rotation_cos = std::cos(environment->rotation_radians),
                            .rotation_sin = std::sin(environment->rotation_radians),
                            .intensity = environment->intensity,
                            .prefiltered_max_mip = environment->prefiltered_max_mip};
  }
  if (lighting::pack_view_lights(snapshot, view_index, limits, result.lights,
                                 result.requirements) != lighting::light_pack_error::none) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  output = std::move(result);
  return GRANIT_SUCCESS;
}

} // namespace granit::pipeline::detail
