// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/environment_resources.h"

#include <array>

namespace granit::example::model_viewer {
namespace {

constexpr std::uint32_t rgba16_bytes_per_pixel = 8;

granit::result upload_cube_mip(granit::texture& texture, std::span<const std::byte> pixels,
                               std::uint32_t resolution, std::uint32_t mip) noexcept {
  const auto face_size =
      std::size_t{resolution} * resolution * rgba16_bytes_per_pixel;
  if (pixels.size() != face_size * 6)
    return granit::result::invalid_argument;
  // 逐面上传让每次写入的行跨度与层范围完全独立，避免后端解释数组层跨度的差异。
  for (std::uint32_t face = 0; face < 6; ++face) {
    const auto result = texture.write(
        pixels.subspan(std::size_t{face} * face_size, face_size),
        {.bytes_per_row = resolution * rgba16_bytes_per_pixel},
        {.mip_level = mip,
         .base_array_layer = face,
         .array_layer_count = 1,
         .width = resolution,
         .height = resolution});
    if (granit::failed(result))
      return result;
  }
  return granit::result::success;
}

} // namespace

granit::result environment_resources::initialize(granit_renderer renderer,
                                                 const environment_package& package) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || valid() || package.irradiance_resolution == 0 ||
      package.irradiance_pixels.empty() || package.prefiltered_mips.empty() ||
      package.brdf_width == 0 || package.brdf_height == 0 || package.brdf_pixels.empty()) {
    return granit::result::invalid_argument;
  }

  auto result = irradiance_texture_.initialize(
      renderer,
      {.dimension = granit::texture_dimension::cube,
       .format = granit::texture_format::rgba16_float,
       .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
       .width = package.irradiance_resolution,
       .height = package.irradiance_resolution,
       .array_layers = 6});
  if (granit::succeeded(result)) {
    result = upload_cube_mip(irradiance_texture_, package.irradiance_pixels,
                             package.irradiance_resolution, 0);
  }
  if (granit::succeeded(result)) {
    result = irradiance_view_.initialize(
        renderer, irradiance_texture_.native_handle(),
        {.dimension = granit::texture_dimension::cube, .array_layer_count = 6});
  }
  if (granit::succeeded(result)) {
    result = prefiltered_texture_.initialize(
        renderer,
        {.dimension = granit::texture_dimension::cube,
         .format = granit::texture_format::rgba16_float,
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
         .width = package.prefiltered_mips.front().resolution,
         .height = package.prefiltered_mips.front().resolution,
         .mip_levels = static_cast<std::uint32_t>(package.prefiltered_mips.size()),
         .array_layers = 6});
  }
  for (std::size_t mip = 0; granit::succeeded(result) && mip < package.prefiltered_mips.size();
       ++mip) {
    result =
        upload_cube_mip(prefiltered_texture_, package.prefiltered_mips[mip].pixels,
                        package.prefiltered_mips[mip].resolution, static_cast<std::uint32_t>(mip));
  }
  if (granit::succeeded(result)) {
    result = prefiltered_view_.initialize(
        renderer, prefiltered_texture_.native_handle(),
        {.dimension = granit::texture_dimension::cube,
         .mip_level_count = static_cast<std::uint32_t>(package.prefiltered_mips.size()),
         .array_layer_count = 6});
  }
  if (granit::succeeded(result)) {
    result =
        brdf_texture_.initialize(renderer, {.format = granit::texture_format::rgba16_float,
                                            .usage = granit::texture_usage::sampled |
                                                     granit::texture_usage::transfer_destination,
                                            .width = package.brdf_width,
                                            .height = package.brdf_height});
  }
  if (granit::succeeded(result)) {
    result = brdf_texture_.write(package.brdf_pixels,
                                 {.bytes_per_row = package.brdf_width * rgba16_bytes_per_pixel},
                                 {.width = package.brdf_width, .height = package.brdf_height});
  }
  if (granit::succeeded(result))
    result = brdf_view_.initialize(renderer, brdf_texture_.native_handle());
  if (granit::failed(result)) {
    reset();
    return result;
  }

  environment_.irradiance = irradiance_view_.native_handle();
  environment_.prefiltered_environment = prefiltered_view_.native_handle();
  environment_.brdf_lut = brdf_view_.native_handle();
  environment_.prefiltered_max_mip = static_cast<float>(package.prefiltered_mips.size() - 1U);
  return granit::result::success;
}

granit::result environment_resources::initialize_builtin_studio(granit_renderer renderer) noexcept {
  // 六个面分别提供暖主光、冷填充、顶部柔光、地面暗部和前后轮廓光。
  constexpr std::array<std::uint16_t, 24> irradiance{
      0x3266, 0x319a, 0x30cd, 0x3c00, 0x2e66, 0x2f5c, 0x30cd, 0x3c00,
      0x359a, 0x3571, 0x351f, 0x3c00, 0x291f, 0x291f, 0x2a66, 0x3c00,
      0x2fae, 0x307b, 0x311f, 0x3c00, 0x2e66, 0x2d1f, 0x2c7b, 0x3c00};
  constexpr std::array<std::uint16_t, 24> prefiltered{
      0x38cd, 0x3866, 0x3800, 0x3c00, 0x3400, 0x34cd, 0x35ae, 0x3c00,
      0x399a, 0x3971, 0x391f, 0x3c00, 0x2e66, 0x2e66, 0x2fae, 0x3c00,
      0x35ae, 0x3666, 0x3733, 0x3c00, 0x3400, 0x3266, 0x319a, 0x3c00};
  constexpr std::array<std::uint16_t, 4> brdf_lut{0x3800, 0x2e66, 0x0000, 0x3c00};
  const environment_mip prefiltered_mip{1, std::as_bytes(std::span{prefiltered})};
  environment_package package;
  package.irradiance_resolution = 1;
  package.irradiance_pixels = std::as_bytes(std::span{irradiance});
  package.prefiltered_mips = {prefiltered_mip};
  package.brdf_width = 1;
  package.brdf_height = 1;
  package.brdf_pixels = std::as_bytes(std::span{brdf_lut});
  return initialize(renderer, package);
}

void environment_resources::reset() noexcept {
  environment_ = GRANIT_RENDER_PIPELINE_ENVIRONMENT_INIT;
  static_cast<void>(brdf_view_.reset());
  static_cast<void>(brdf_texture_.reset());
  static_cast<void>(prefiltered_view_.reset());
  static_cast<void>(prefiltered_texture_.reset());
  static_cast<void>(irradiance_view_.reset());
  static_cast<void>(irradiance_texture_.reset());
}

} // namespace granit::example::model_viewer
