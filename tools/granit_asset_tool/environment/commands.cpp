// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "granit_asset_tool/environment/commands.h"

#include "granit_asset_tool/file_io.h"

#include <granit/asset_tools/environment_builder.hpp>

#include <charconv>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::asset_tools::cli {
namespace {

struct mip_source {
  std::uint32_t resolution{};
  std::filesystem::path path;
  std::vector<std::byte> pixels;
};

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_asset_tool environment build --irradiance <rgba16f.bin> "
               "--irradiance-resolution <n> --prefiltered <resolution=rgba16f.bin>... "
               "--brdf-lut <rgba16f.bin> --brdf-width <n> --brdf-height <n> "
               "--output <environment.grenv> [--intensity <值>] [--exposure <EV>]\n"
               "  granit_asset_tool environment inspect <environment.grenv> --json "
               "[--output <debug.json>]\n";
}

std::optional<std::string_view> option_value(int argc, char** argv, std::string_view option) {
  for (int index = 2; index + 1 < argc; ++index) {
    if (std::string_view{argv[index]} == option)
      return argv[index + 1];
  }
  return std::nullopt;
}

std::vector<std::string_view> option_values(int argc, char** argv, std::string_view option) {
  std::vector<std::string_view> values;
  for (int index = 2; index + 1 < argc; ++index) {
    if (std::string_view{argv[index]} == option)
      values.emplace_back(argv[index + 1]);
  }
  return values;
}

std::optional<std::uint32_t> parse_u32(std::string_view text) {
  std::uint32_t value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value == 0)
    return std::nullopt;
  return value;
}

std::optional<float> parse_float(std::string_view text) {
  float value = 0.0F;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value))
    return std::nullopt;
  return value;
}

int inspect_environment(int argc, char** argv) {
  if ((argc != 4 && argc != 6) || std::string_view{argv[3]} != "--json" ||
      (argc == 6 && std::string_view{argv[4]} != "--output")) {
    print_usage();
    return 2;
  }
  const auto package = granit::asset_tools::cli::read_file(argv[2]);
  if (package.empty()) {
    std::cerr << "无法读取 Environment Asset\n";
    return 1;
  }
  auto [status, result] = granit::asset_tools::environment::inspect(package);
  if (status.failed()) {
    std::cerr << result.diagnostic();
    return 1;
  }
  if (argc == 6) {
    if (!granit::asset_tools::cli::write_file_atomic(argv[5], result.debug_json())) {
      std::cerr << "无法原子写入调试 JSON\n";
      return 1;
    }
  } else {
    std::cout << result.debug_json();
  }
  return 0;
}

int build_environment(int argc, char** argv) {
  const auto irradiance_path = option_value(argc, argv, "--irradiance");
  const auto irradiance_resolution_text = option_value(argc, argv, "--irradiance-resolution");
  const auto brdf_path = option_value(argc, argv, "--brdf-lut");
  const auto brdf_width_text = option_value(argc, argv, "--brdf-width");
  const auto brdf_height_text = option_value(argc, argv, "--brdf-height");
  const auto output = option_value(argc, argv, "--output");
  const auto prefiltered_specs = option_values(argc, argv, "--prefiltered");
  const auto irradiance_resolution =
      irradiance_resolution_text ? parse_u32(*irradiance_resolution_text) : std::nullopt;
  const auto brdf_width = brdf_width_text ? parse_u32(*brdf_width_text) : std::nullopt;
  const auto brdf_height = brdf_height_text ? parse_u32(*brdf_height_text) : std::nullopt;
  const auto intensity = parse_float(option_value(argc, argv, "--intensity").value_or("0.12"));
  const auto exposure = parse_float(option_value(argc, argv, "--exposure").value_or("-0.5"));
  if (!irradiance_path || !irradiance_resolution || !brdf_path || !brdf_width || !brdf_height ||
      !output || prefiltered_specs.empty() || !intensity || *intensity < 0.0F || !exposure ||
      *exposure < -24.0F || *exposure > 24.0F) {
    print_usage();
    return 2;
  }

  const auto irradiance = granit::asset_tools::cli::read_file(*irradiance_path);
  const auto brdf = granit::asset_tools::cli::read_file(*brdf_path);
  if (irradiance.empty() || brdf.empty()) {
    std::cerr << "无法读取 Environment Asset 像素输入\n";
    return 1;
  }
  std::vector<mip_source> sources;
  for (const auto spec : prefiltered_specs) {
    const auto separator = spec.find('=');
    const auto resolution =
        separator == std::string_view::npos ? std::nullopt : parse_u32(spec.substr(0, separator));
    if (!resolution || separator + 1 >= spec.size()) {
      std::cerr << "无效 Prefiltered mip：" << spec << '\n';
      return 2;
    }
    mip_source source{*resolution, std::string{spec.substr(separator + 1)}, {}};
    source.pixels = granit::asset_tools::cli::read_file(source.path);
    if (source.pixels.empty()) {
      std::cerr << "无法读取 Prefiltered mip：" << source.path << '\n';
      return 1;
    }
    sources.push_back(std::move(source));
  }
  std::vector<granit::asset_tools::environment::mip_desc> mips;
  mips.reserve(sources.size());
  for (const auto& source : sources)
    mips.push_back({source.resolution, source.pixels});
  auto [status, result] =
      granit::asset_tools::environment::build({*intensity, *exposure, *irradiance_resolution,
                                               irradiance, mips, *brdf_width, *brdf_height, brdf});
  if (status.failed()) {
    std::cerr << result.diagnostic();
    return 1;
  }
  if (!granit::asset_tools::cli::write_file_atomic(*output, result.package())) {
    std::cerr << "无法原子写入 Environment Asset\n";
    return 1;
  }
  return 0;
}

} // namespace

int run_environment_command(int argc, char** argv) {
  if (argc >= 2 && std::string_view{argv[1]} == "build")
    return build_environment(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "inspect")
    return inspect_environment(argc, argv);
  print_usage();
  return 2;
}

} // namespace granit::asset_tools::cli
