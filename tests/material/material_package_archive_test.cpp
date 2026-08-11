// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_package_archive.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using namespace granit::material;

material_package make_package(bool reverse_parameters) {
  material_package_desc desc;
  desc.metadata.constant_buffer_size = 32;
  desc.metadata.parameters = {
      {.name = "base_color", .type = parameter_type::float4, .offset = 0, .default_value = {}},
      {.name = "roughness", .type = parameter_type::float32, .offset = 16, .default_value = {}},
  };
  if (reverse_parameters) {
    std::ranges::reverse(desc.metadata.parameters);
  }
  constexpr std::uint32_t spirv_magic = UINT32_C(0x07230203);
  desc.variants.push_back({.pass = make_feature_id("opaque"),
                           .features = {{make_feature_id("normal_map"), 1}},
                           .shaders = {{.stage = package_shader_stage::fragment,
                                        .entry_point = "fragment_main",
                                        .spirv = {spirv_magic, UINT32_C(0x00010600), 0, 1, 0}},
                                       {.stage = package_shader_stage::vertex,
                                        .entry_point = "vertex_main",
                                        .spirv = {spirv_magic, UINT32_C(0x00010600), 0, 1, 0}}}});
  material_package package;
  REQUIRE(material_package::build(std::move(desc), package) == package_error::none);
  return package;
}

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
  return std::to_integer<std::uint32_t>(bytes[offset]) |
         (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 3]) << 24U);
}

} // namespace

TEST_CASE("材质包语义数据编码到独立归档区段") {
  const auto package = make_package(false);
  std::vector<std::byte> bytes;
  REQUIRE(encode_material_package_archive(package, bytes) == archive_error::none);

  material_archive_layout layout;
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);
  REQUIRE(layout.sections.size() == 7);
  const auto section = [&](archive_section_type type) -> std::span<const std::byte> {
    const auto found = std::ranges::find(layout.sections, static_cast<std::uint32_t>(type),
                                         &material_archive_section::type);
    REQUIRE(found != layout.sections.end());
    return std::span{bytes}.subspan(found->offset, found->stored_size);
  };
  CHECK(read_u32(section(archive_section_type::material_metadata), 4) == 2);
  CHECK(read_u32(section(archive_section_type::feature_definitions), 0) == 1);
  CHECK(read_u32(section(archive_section_type::pass_definitions), 0) == 1);
  CHECK(read_u32(section(archive_section_type::variant_records), 0) == 1);
  CHECK(read_u32(section(archive_section_type::shader_records), 0) == 2);
  CHECK(section(archive_section_type::spirv_data).size() == 40);
}

TEST_CASE("材质包编码不依赖参数和 Shader 输入顺序") {
  const auto first_package = make_package(false);
  const auto second_package = make_package(true);
  std::vector<std::byte> first;
  std::vector<std::byte> second;
  REQUIRE(encode_material_package_archive(first_package, first) == archive_error::none);
  REQUIRE(encode_material_package_archive(second_package, second) == archive_error::none);
  CHECK(first == second);
}
