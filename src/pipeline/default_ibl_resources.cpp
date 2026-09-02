// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/default_ibl_resources.h"

#include <array>
#include <span>

namespace granit::pipeline::detail {

granit_result default_ibl_resources::initialize(granit_renderer renderer) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || initialized())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const granit::texture_desc cube_desc{.dimension = granit::texture_dimension::cube,
                                       .format = granit::texture_format::rgba16_float,
                                       .usage = granit::texture_usage::sampled |
                                                granit::texture_usage::transfer_destination,
                                       .width = 1,
                                       .height = 1,
                                       .array_layers = 6};
  auto result = irradiance_texture_.initialize(renderer, cube_desc);
  if (granit::succeeded(result))
    result = prefiltered_texture_.initialize(renderer, cube_desc);
  if (granit::succeeded(result)) {
    result = brdf_lut_texture_.initialize(
        renderer,
        {.format = granit::texture_format::rgba16_float,
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination});
  }
  constexpr std::array<std::uint16_t, 24> black_cube{};
  // 黑环境使 IBL 能量为零；BRDF LUT 保留一个有效中性近似值，便于后续替换环境纹理。
  constexpr std::array<std::uint16_t, 4> neutral_lut{0x3800, 0x2e66, 0x0000, 0x3c00};
  if (granit::succeeded(result)) {
    result = irradiance_texture_.write(std::as_bytes(std::span{black_cube}),
                                       {.bytes_per_row = 8, .rows_per_image = 1},
                                       {.array_layer_count = 6});
  }
  if (granit::succeeded(result)) {
    result = prefiltered_texture_.write(std::as_bytes(std::span{black_cube}),
                                        {.bytes_per_row = 8, .rows_per_image = 1},
                                        {.array_layer_count = 6});
  }
  if (granit::succeeded(result)) {
    result =
        brdf_lut_texture_.write(std::as_bytes(std::span{neutral_lut}), {.bytes_per_row = 8}, {});
  }
  const granit::texture_view_desc cube_view{.dimension = granit::texture_dimension::cube,
                                            .array_layer_count = 6};
  if (granit::succeeded(result)) {
    result = irradiance_view_.initialize(renderer, irradiance_texture_.native_handle(), cube_view);
  }
  if (granit::succeeded(result)) {
    result =
        prefiltered_view_.initialize(renderer, prefiltered_texture_.native_handle(), cube_view);
  }
  if (granit::succeeded(result))
    result = brdf_lut_view_.initialize(renderer, brdf_lut_texture_.native_handle());
  if (granit::succeeded(result)) {
    result = granit::from_native(
        resources_.initialize(renderer,
                              {.irradiance = irradiance_view_.native_handle(),
                               .prefiltered_environment = prefiltered_view_.native_handle(),
                               .brdf_lut = brdf_lut_view_.native_handle()},
                              {.intensity = 0.0F}));
  }
  if (granit::failed(result))
    static_cast<void>(reset());
  return static_cast<granit_result>(result);
}

granit_result default_ibl_resources::reset() noexcept {
  granit_result first = resources_.reset();
  const auto capture = [&](granit::result result) {
    if (first == GRANIT_SUCCESS && granit::failed(result))
      first = static_cast<granit_result>(result);
  };
  capture(brdf_lut_view_.reset());
  capture(prefiltered_view_.reset());
  capture(irradiance_view_.reset());
  capture(brdf_lut_texture_.reset());
  capture(prefiltered_texture_.reset());
  capture(irradiance_texture_.reset());
  return first;
}

} // namespace granit::pipeline::detail
