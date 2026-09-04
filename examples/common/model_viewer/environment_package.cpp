// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/environment_package.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <new>

namespace granit::example::model_viewer {
namespace {

constexpr std::array magic{'G', 'R', 'E', 'N', 'V', '0', '2', '\0'};
constexpr std::size_t header_size = 64;
constexpr std::uint32_t version = 2;
constexpr std::uint32_t rgba16_float = 1;
constexpr std::uint64_t bytes_per_pixel = 8;
constexpr std::uint64_t cube_face_count = 6;
constexpr std::uint32_t maximum_resolution = 4096;

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < 4; ++index)
    value |= std::to_integer<std::uint32_t>(bytes[offset + index]) << (index * 8U);
  return value;
}

std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < 8; ++index)
    value |= std::to_integer<std::uint64_t>(bytes[offset + index]) << (index * 8U);
  return value;
}

void write_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) noexcept {
  for (std::size_t index = 0; index < 4; ++index)
    bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
}

void write_u64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value) noexcept {
  for (std::size_t index = 0; index < 8; ++index)
    bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
}

float read_f32(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  return std::bit_cast<float>(read_u32(bytes, offset));
}

void write_f32(std::vector<std::byte>& bytes, std::size_t offset, float value) noexcept {
  write_u32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

bool valid_resolution(std::uint32_t value) noexcept {
  return value != 0 && value <= maximum_resolution && (value & (value - 1U)) == 0;
}

bool append_size(std::uint64_t& total, std::uint32_t width, std::uint32_t height,
                 std::uint64_t layers) noexcept {
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  if (width > maximum / height || static_cast<std::uint64_t>(width) * height > maximum / layers ||
      static_cast<std::uint64_t>(width) * height * layers > maximum / bytes_per_pixel) {
    return false;
  }
  const auto size = static_cast<std::uint64_t>(width) * height * layers * bytes_per_pixel;
  if (total > maximum - size)
    return false;
  total += size;
  return true;
}

} // namespace

environment_package_error parse_environment_package(std::span<const std::byte> bytes,
                                                    environment_package& package) {
  package = {};
  if (bytes.size() < header_size)
    return environment_package_error::truncated;
  for (std::size_t index = 0; index < magic.size(); ++index) {
    if (std::to_integer<char>(bytes[index]) != magic[index])
      return environment_package_error::invalid_magic;
  }
  if (read_u32(bytes, 8) != version)
    return environment_package_error::unsupported_version;
  if (read_u32(bytes, 12) != header_size || read_u32(bytes, 16) != rgba16_float ||
      read_u32(bytes, 56) != 0 || read_u32(bytes, 60) != 0) {
    return environment_package_error::invalid_layout;
  }

  const auto irradiance_resolution = read_u32(bytes, 20);
  const auto prefiltered_resolution = read_u32(bytes, 24);
  const auto mip_count = read_u32(bytes, 28);
  const auto brdf_width = read_u32(bytes, 32);
  const auto brdf_height = read_u32(bytes, 36);
  const auto recommended_environment_intensity = read_f32(bytes, 48);
  const auto recommended_exposure_ev = read_f32(bytes, 52);
  if (!valid_resolution(irradiance_resolution) || !valid_resolution(prefiltered_resolution) ||
      !valid_resolution(brdf_width) || !valid_resolution(brdf_height) || mip_count == 0 ||
      mip_count > 13 || !std::isfinite(recommended_environment_intensity) ||
      recommended_environment_intensity < 0.0F || !std::isfinite(recommended_exposure_ev) ||
      recommended_exposure_ev < -24.0F || recommended_exposure_ev > 24.0F) {
    return environment_package_error::invalid_layout;
  }
  std::uint32_t maximum_mip_count = 1;
  for (auto resolution = prefiltered_resolution; resolution > 1; resolution /= 2U)
    ++maximum_mip_count;
  if (mip_count > maximum_mip_count)
    return environment_package_error::invalid_layout;

  std::uint64_t expected_payload = 0;
  if (!append_size(expected_payload, irradiance_resolution, irradiance_resolution,
                   cube_face_count)) {
    return environment_package_error::size_overflow;
  }
  auto mip_resolution = prefiltered_resolution;
  for (std::uint32_t mip = 0; mip < mip_count; ++mip) {
    if (!append_size(expected_payload, mip_resolution, mip_resolution, cube_face_count))
      return environment_package_error::size_overflow;
    mip_resolution = std::max(mip_resolution / 2U, 1U);
  }
  if (!append_size(expected_payload, brdf_width, brdf_height, 1))
    return environment_package_error::size_overflow;
  if (read_u64(bytes, 40) != expected_payload || expected_payload != bytes.size() - header_size) {
    return environment_package_error::invalid_layout;
  }

  try {
    std::size_t offset = header_size;
    const auto irradiance_size =
        static_cast<std::size_t>(static_cast<std::uint64_t>(irradiance_resolution) *
                                 irradiance_resolution * cube_face_count * bytes_per_pixel);
    package.irradiance_resolution = irradiance_resolution;
    package.recommended_environment_intensity = recommended_environment_intensity;
    package.recommended_exposure_ev = recommended_exposure_ev;
    package.irradiance_pixels = bytes.subspan(offset, irradiance_size);
    offset += irradiance_size;
    mip_resolution = prefiltered_resolution;
    package.prefiltered_mips.reserve(mip_count);
    for (std::uint32_t mip = 0; mip < mip_count; ++mip) {
      const auto mip_size =
          static_cast<std::size_t>(static_cast<std::uint64_t>(mip_resolution) * mip_resolution *
                                   cube_face_count * bytes_per_pixel);
      package.prefiltered_mips.push_back(
          {.resolution = mip_resolution, .pixels = bytes.subspan(offset, mip_size)});
      offset += mip_size;
      mip_resolution = std::max(mip_resolution / 2U, 1U);
    }
    package.brdf_width = brdf_width;
    package.brdf_height = brdf_height;
    package.brdf_pixels = bytes.subspan(offset);
    return environment_package_error::none;
  } catch (const std::bad_alloc&) {
    package = {};
    return environment_package_error::size_overflow;
  }
}

environment_package_error encode_environment_package(const environment_package& package,
                                                     std::vector<std::byte>& output) {
  if (!valid_resolution(package.irradiance_resolution) || package.prefiltered_mips.empty() ||
      !valid_resolution(package.prefiltered_mips.front().resolution) ||
      !valid_resolution(package.brdf_width) || !valid_resolution(package.brdf_height) ||
      package.prefiltered_mips.size() > 13 ||
      !std::isfinite(package.recommended_environment_intensity) ||
      package.recommended_environment_intensity < 0.0F ||
      !std::isfinite(package.recommended_exposure_ev) || package.recommended_exposure_ev < -24.0F ||
      package.recommended_exposure_ev > 24.0F) {
    return environment_package_error::invalid_layout;
  }

  std::uint64_t payload_size = 0;
  if (!append_size(payload_size, package.irradiance_resolution, package.irradiance_resolution,
                   cube_face_count) ||
      package.irradiance_pixels.size() != payload_size) {
    return environment_package_error::invalid_layout;
  }
  auto resolution = package.prefiltered_mips.front().resolution;
  for (const auto& mip : package.prefiltered_mips) {
    const auto before = payload_size;
    if (mip.resolution != resolution ||
        !append_size(payload_size, resolution, resolution, cube_face_count) ||
        mip.pixels.size() != payload_size - before) {
      return environment_package_error::invalid_layout;
    }
    resolution = std::max(resolution / 2U, 1U);
  }
  const auto before_brdf = payload_size;
  if (!append_size(payload_size, package.brdf_width, package.brdf_height, 1) ||
      package.brdf_pixels.size() != payload_size - before_brdf ||
      payload_size > std::numeric_limits<std::size_t>::max() - header_size) {
    return environment_package_error::invalid_layout;
  }

  try {
    std::vector<std::byte> candidate(header_size + static_cast<std::size_t>(payload_size));
    for (std::size_t index = 0; index < magic.size(); ++index)
      candidate[index] = static_cast<std::byte>(magic[index]);
    write_u32(candidate, 8, version);
    write_u32(candidate, 12, static_cast<std::uint32_t>(header_size));
    write_u32(candidate, 16, rgba16_float);
    write_u32(candidate, 20, package.irradiance_resolution);
    write_u32(candidate, 24, package.prefiltered_mips.front().resolution);
    write_u32(candidate, 28, static_cast<std::uint32_t>(package.prefiltered_mips.size()));
    write_u32(candidate, 32, package.brdf_width);
    write_u32(candidate, 36, package.brdf_height);
    write_u64(candidate, 40, payload_size);
    write_f32(candidate, 48, package.recommended_environment_intensity);
    write_f32(candidate, 52, package.recommended_exposure_ev);
    auto destination = candidate.begin() + static_cast<std::ptrdiff_t>(header_size);
    destination = std::ranges::copy(package.irradiance_pixels, destination).out;
    for (const auto& mip : package.prefiltered_mips)
      destination = std::ranges::copy(mip.pixels, destination).out;
    std::ranges::copy(package.brdf_pixels, destination);
    output = std::move(candidate);
    return environment_package_error::none;
  } catch (const std::bad_alloc&) {
    return environment_package_error::size_overflow;
  }
}

} // namespace granit::example::model_viewer
