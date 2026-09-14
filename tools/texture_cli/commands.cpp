// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "texture_cli/commands.h"

#include "asset_file_io.h"
#include "core/texture_format.h"

#include <granit/asset_tools/texture_builder.hpp>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct variant_source {
  granit_texture_format format{};
  std::filesystem::path path;
  std::vector<std::byte> payload;
  std::vector<granit_texture_asset_subresource_info> subresources;
};

struct texture_shape {
  granit_texture_dimension dimension{};
  uint32_t width{};
  uint32_t height{};
  uint32_t depth{};
  uint32_t array_layers{};
  uint32_t mip_levels{};
};

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_asset_tool texture build --output <manifest.grtex> "
               "--payload-output <payload.bin> --dimension <1d|2d|3d|cube> "
               "--width <n> --height <n> --depth <n> --layers <n> --mips <n> "
               "[--usage <sampled,transfer-destination,...>] "
               "--variant <format=payload.bin>...\n"
               "  granit_asset_tool texture inspect <manifest.grtex> --json "
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

std::optional<uint32_t> parse_u32(std::optional<std::string_view> text) {
  if (!text)
    return std::nullopt;
  uint32_t value = 0;
  const auto parsed = std::from_chars(text->data(), text->data() + text->size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size() || value == 0)
    return std::nullopt;
  return value;
}

std::optional<granit_texture_dimension> parse_dimension(std::string_view value) {
  if (value == "1d")
    return GRANIT_TEXTURE_DIMENSION_1D;
  if (value == "2d")
    return GRANIT_TEXTURE_DIMENSION_2D;
  if (value == "3d")
    return GRANIT_TEXTURE_DIMENSION_3D;
  if (value == "cube")
    return GRANIT_TEXTURE_DIMENSION_CUBE;
  return std::nullopt;
}

std::optional<granit_texture_format> parse_format(std::string_view value) {
  static constexpr std::pair<std::string_view, granit_texture_format> formats[]{
      {"r8-unorm", GRANIT_TEXTURE_FORMAT_R8_UNORM},
      {"rg8-unorm", GRANIT_TEXTURE_FORMAT_RG8_UNORM},
      {"rgba8-unorm", GRANIT_TEXTURE_FORMAT_RGBA8_UNORM},
      {"rgba8-srgb", GRANIT_TEXTURE_FORMAT_RGBA8_SRGB},
      {"bgra8-unorm", GRANIT_TEXTURE_FORMAT_BGRA8_UNORM},
      {"bgra8-srgb", GRANIT_TEXTURE_FORMAT_BGRA8_SRGB},
      {"rgba16-float", GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT},
      {"bc1-rgba-unorm", GRANIT_TEXTURE_FORMAT_BC1_RGBA_UNORM},
      {"bc1-rgba-srgb", GRANIT_TEXTURE_FORMAT_BC1_RGBA_SRGB},
      {"bc3-rgba-unorm", GRANIT_TEXTURE_FORMAT_BC3_RGBA_UNORM},
      {"bc3-rgba-srgb", GRANIT_TEXTURE_FORMAT_BC3_RGBA_SRGB},
      {"bc5-rg-unorm", GRANIT_TEXTURE_FORMAT_BC5_RG_UNORM},
      {"bc7-rgba-unorm", GRANIT_TEXTURE_FORMAT_BC7_RGBA_UNORM},
      {"bc7-rgba-srgb", GRANIT_TEXTURE_FORMAT_BC7_RGBA_SRGB},
      {"etc2-rgba8-unorm", GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_UNORM},
      {"etc2-rgba8-srgb", GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_SRGB},
      {"astc-4x4-unorm", GRANIT_TEXTURE_FORMAT_ASTC_4X4_UNORM},
      {"astc-4x4-srgb", GRANIT_TEXTURE_FORMAT_ASTC_4X4_SRGB},
  };
  const auto found =
      std::ranges::find_if(formats, [value](const auto& item) { return item.first == value; });
  return found == std::end(formats) ? std::nullopt
                                    : std::optional<granit_texture_format>{found->second};
}

std::optional<granit_texture_usage> parse_usage(std::string_view value) {
  granit_texture_usage usage = 0;
  while (!value.empty()) {
    const auto separator = value.find(',');
    const auto token = value.substr(0, separator);
    if (token == "sampled")
      usage |= GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
    else if (token == "transfer-source")
      usage |= GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT;
    else if (token == "transfer-destination")
      usage |= GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
    else if (token == "storage")
      usage |= GRANIT_TEXTURE_USAGE_STORAGE_BIT;
    else if (token == "color-attachment")
      usage |= GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
    else
      return std::nullopt;
    if (separator == std::string_view::npos)
      break;
    value.remove_prefix(separator + 1);
  }
  return usage == 0 ? std::nullopt : std::optional<granit_texture_usage>{usage};
}

uint32_t mip_extent(uint32_t extent, uint32_t mip) {
  return std::max(UINT32_C(1), extent >> std::min(mip, UINT32_C(31)));
}

bool append_subresources(std::vector<granit_texture_asset_subresource_info>& subresources,
                         const texture_shape& shape, granit_texture_format format,
                         std::size_t payload_size) {
  uint64_t offset = 0;
  for (uint32_t layer = 0; layer < shape.array_layers; ++layer) {
    for (uint32_t mip = 0; mip < shape.mip_levels; ++mip) {
      const auto depth = shape.dimension == GRANIT_TEXTURE_DIMENSION_3D
                             ? mip_extent(shape.depth, mip)
                             : UINT32_C(1);
      granit_texture_data_layout layout{offset, 0, 0};
      granit::detail::texture_transfer_footprint footprint{};
      if (!granit::detail::calculate_texture_transfer_footprint(
              format, mip_extent(shape.width, mip), mip_extent(shape.height, mip), depth, layout,
              footprint) ||
          offset > payload_size || footprint.required_size > payload_size - offset)
        return false;
      subresources.push_back({mip,
                              layer,
                              offset,
                              footprint.required_size,
                              static_cast<uint32_t>(footprint.row_pitch),
                              static_cast<uint32_t>(footprint.image_rows),
                              {0, 0}});
      offset += footprint.required_size;
    }
  }
  return offset == payload_size;
}

int inspect_texture(int argc, char** argv) {
  if ((argc != 4 && argc != 6) || std::string_view{argv[3]} != "--json" ||
      (argc == 6 && std::string_view{argv[4]} != "--output")) {
    print_usage();
    return 2;
  }
  const auto manifest = granit::asset_tools::cli::read_file(argv[2]);
  if (manifest.empty()) {
    std::cerr << "无法读取 Texture Asset Manifest\n";
    return 1;
  }
  auto [status, result] = granit::asset_tools::texture::inspect(manifest);
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

int build_texture(int argc, char** argv) {
  const auto output = option_value(argc, argv, "--output");
  const auto payload_output = option_value(argc, argv, "--payload-output");
  const auto dimension_text = option_value(argc, argv, "--dimension");
  const auto width = parse_u32(option_value(argc, argv, "--width"));
  const auto height = parse_u32(option_value(argc, argv, "--height"));
  const auto depth = parse_u32(option_value(argc, argv, "--depth"));
  const auto layers = parse_u32(option_value(argc, argv, "--layers"));
  const auto mips = parse_u32(option_value(argc, argv, "--mips"));
  const auto usage =
      parse_usage(option_value(argc, argv, "--usage").value_or("sampled,transfer-destination"));
  const auto variants = option_values(argc, argv, "--variant");
  const auto dimension = dimension_text ? parse_dimension(*dimension_text) : std::nullopt;
  if (!output || !payload_output || !dimension || !width || !height || !depth || !layers || !mips ||
      !usage || variants.empty()) {
    print_usage();
    return 2;
  }

  const texture_shape shape{*dimension, *width, *height, *depth, *layers, *mips};
  std::vector<variant_source> sources;
  for (const auto spec : variants) {
    const auto separator = spec.find('=');
    const auto format = separator == std::string_view::npos
                            ? std::nullopt
                            : parse_format(spec.substr(0, separator));
    if (!format || separator + 1 >= spec.size()) {
      std::cerr << "无效 Texture 变体：" << spec << '\n';
      return 2;
    }
    variant_source source{*format, std::string{spec.substr(separator + 1)}, {}, {}};
    source.payload = granit::asset_tools::cli::read_file(source.path);
    if (source.payload.empty()) {
      std::cerr << "无法读取 Texture 变体负载：" << source.path << '\n';
      return 1;
    }
    if (!append_subresources(source.subresources, shape, source.format, source.payload.size())) {
      std::cerr << "Texture 变体负载大小与完整 mip/层布局不一致：" << source.path << '\n';
      return 1;
    }
    sources.push_back(std::move(source));
  }
  std::vector<granit::asset_tools::texture::variant_desc> variant_descs;
  variant_descs.reserve(sources.size());
  for (const auto& source : sources)
    variant_descs.push_back({source.format, *usage, source.payload, source.subresources});
  auto [status, result] =
      granit::asset_tools::texture::build({shape.dimension, shape.width, shape.height, shape.depth,
                                           shape.array_layers, shape.mip_levels, variant_descs});
  if (status.failed()) {
    std::cerr << result.diagnostic();
    return 1;
  }
  if (!granit::asset_tools::cli::write_file_atomic(*payload_output, result.payload()) ||
      !granit::asset_tools::cli::write_file_atomic(*output, result.manifest())) {
    std::cerr << "无法原子写入 Texture Asset 输出\n";
    return 1;
  }
  return 0;
}

} // namespace

int run_texture_command(int argc, char** argv) {
  if (argc >= 2 && std::string_view{argv[1]} == "build")
    return build_texture(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "inspect")
    return inspect_texture(argc, argv);
  print_usage();
  return 2;
}
