// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/image_decoder.h"
#include "model_viewer/environment_ktx2.h"
#include "model_viewer/environment_package.h"

#include <bit>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint64_t maximum_input_size = UINT64_C(1) << 30U;

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return {};
  const auto end = stream.tellg();
  if (end <= 0 || static_cast<std::uint64_t>(end) > maximum_input_size)
    return {};
  std::vector<std::byte> bytes(static_cast<std::size_t>(end));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), end);
  return stream ? bytes : std::vector<std::byte>{};
}

bool write_file(const std::filesystem::path& path, std::span<const std::byte> bytes) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream)
    return false;
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(stream);
}

std::uint16_t float_to_half(float value) noexcept {
  const auto bits = std::bit_cast<std::uint32_t>(value);
  const auto sign = static_cast<std::uint16_t>((bits >> 16U) & 0x8000U);
  const auto exponent = static_cast<int>((bits >> 23U) & 0xffU) - 127 + 15;
  auto mantissa = bits & 0x7fffffU;
  if (exponent <= 0) {
    if (exponent < -10)
      return sign;
    mantissa = (mantissa | 0x800000U) >> static_cast<unsigned>(1 - exponent);
    return static_cast<std::uint16_t>(sign | ((mantissa + 0x1000U) >> 13U));
  }
  if (exponent >= 31)
    return static_cast<std::uint16_t>(sign | 0x7c00U);
  mantissa += 0x1000U;
  if ((mantissa & 0x800000U) != 0)
    return float_to_half(std::bit_cast<float>((bits & 0x80000000U) |
                                              (static_cast<std::uint32_t>(exponent + 113) << 23U)));
  return static_cast<std::uint16_t>(sign | (static_cast<std::uint16_t>(exponent) << 10U) |
                                    static_cast<std::uint16_t>(mantissa >> 13U));
}

std::vector<std::byte> convert_lut(const granit::example::gltf::image& image) {
  std::vector<std::byte> output(image.rgba8_pixels.size() * 2U);
  for (std::size_t index = 0; index < image.rgba8_pixels.size(); ++index) {
    const auto normalized = std::to_integer<std::uint8_t>(image.rgba8_pixels[index]) / 255.0F;
    const auto half = float_to_half(normalized);
    output[index * 2U] = static_cast<std::byte>(half & 0xffU);
    output[index * 2U + 1U] = static_cast<std::byte>(half >> 8U);
  }
  return output;
}

void print_usage() {
  std::cerr << "用法：granit_model_viewer_environment_tool build "
               "--irradiance <diffuse.ktx2> --prefiltered <specular.ktx2> "
               "--brdf-lut <lut.png> --output <environment.grenv> "
               "[--intensity <强度>] [--exposure <EV>]\n";
}

bool parse_float(std::string_view text, float& value) noexcept {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
         std::isfinite(value);
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 10 || argc % 2 != 0 || std::string_view{argv[1]} != "build") {
    print_usage();
    return 2;
  }
  std::filesystem::path irradiance_path;
  std::filesystem::path prefiltered_path;
  std::filesystem::path lut_path;
  std::filesystem::path output_path;
  float recommended_intensity = 0.4F;
  float recommended_exposure = -0.5F;
  for (int index = 2; index < argc; index += 2) {
    const std::string_view option{argv[index]};
    if (option == "--irradiance")
      irradiance_path = argv[index + 1];
    else if (option == "--prefiltered")
      prefiltered_path = argv[index + 1];
    else if (option == "--brdf-lut")
      lut_path = argv[index + 1];
    else if (option == "--output")
      output_path = argv[index + 1];
    else if (option == "--intensity") {
      if (!parse_float(argv[index + 1], recommended_intensity) || recommended_intensity < 0.0F) {
        print_usage();
        return 2;
      }
    } else if (option == "--exposure") {
      if (!parse_float(argv[index + 1], recommended_exposure) || recommended_exposure < -24.0F ||
          recommended_exposure > 24.0F) {
        print_usage();
        return 2;
      }
    } else {
      print_usage();
      return 2;
    }
  }
  if (irradiance_path.empty() || prefiltered_path.empty() || lut_path.empty() ||
      output_path.empty()) {
    print_usage();
    return 2;
  }

  const auto irradiance_bytes = read_file(irradiance_path);
  const auto prefiltered_bytes = read_file(prefiltered_path);
  const auto lut_bytes = read_file(lut_path);
  granit::example::model_viewer::environment_ktx2_cube irradiance;
  granit::example::model_viewer::environment_ktx2_cube prefiltered;
  granit::example::gltf::image lut;
  if (granit::example::model_viewer::parse_environment_ktx2_cube(irradiance_bytes, irradiance) !=
          granit::example::model_viewer::environment_ktx2_error::none ||
      irradiance.levels.size() != 1 ||
      granit::example::model_viewer::parse_environment_ktx2_cube(prefiltered_bytes, prefiltered) !=
          granit::example::model_viewer::environment_ktx2_error::none ||
      prefiltered.levels.back().resolution != 1 ||
      granit::example::gltf::decode_image(lut_bytes, lut) !=
          granit::example::gltf::image_decode_error::none ||
      lut.mips.size() != 1) {
    std::cerr << "环境输入无效或不符合离线格式约束\n";
    return 1;
  }

  auto lut_half = convert_lut(lut);
  granit::example::model_viewer::environment_package package;
  package.recommended_environment_intensity = recommended_intensity;
  package.recommended_exposure_ev = recommended_exposure;
  package.irradiance_resolution = irradiance.levels.front().resolution;
  package.irradiance_pixels = irradiance.levels.front().pixels;
  package.prefiltered_mips.reserve(prefiltered.levels.size());
  for (const auto& level : prefiltered.levels)
    package.prefiltered_mips.push_back({level.resolution, level.pixels});
  package.brdf_width = lut.mips.front().width;
  package.brdf_height = lut.mips.front().height;
  package.brdf_pixels = lut_half;
  std::vector<std::byte> encoded;
  if (granit::example::model_viewer::encode_environment_package(package, encoded) !=
          granit::example::model_viewer::environment_package_error::none ||
      !write_file(output_path, encoded)) {
    std::cerr << "无法编码或写入 GRENV\n";
    return 1;
  }
  return 0;
}
