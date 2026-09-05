// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_debug_json.h"
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
      {.name = "base_color",
       .type = parameter_type::float4,
       .offset = 0,
       .default_value = std::vector<std::byte>(16, std::byte{0x01})},
      {.name = "roughness", .type = parameter_type::float32, .offset = 16, .default_value = {}},
      {.name = "albedo", .type = parameter_type::texture_view, .binding = 1, .default_value = {}},
  };
  if (reverse_parameters) {
    std::ranges::reverse(desc.metadata.parameters);
  }
  constexpr std::uint32_t spirv_magic = UINT32_C(0x07230203);
  desc.variants.push_back({.pass = make_feature_id("opaque"),
                           .features = {{make_feature_id("normal_map"), 1}},
                           .shaders = {{.stage = package_shader_stage::fragment,
                                       .entry_point = "fragment_main",
                                        .asset_id = {std::byte{2}},
                                        .spirv = {spirv_magic, UINT32_C(0x00010600), 0, 1, 0},
                                        .wgsl = "@fragment fn fragment_main() {}"},
                                       {.stage = package_shader_stage::vertex,
                                        .entry_point = "vertex_main",
                                        .asset_id = {std::byte{1}},
                                        .spirv = {spirv_magic, UINT32_C(0x00010600), 0, 1, 0},
                                        .wgsl = "@vertex fn vertex_main() -> @builtin(position) "
                                                "vec4f { return vec4f(); }"}},
                           .pipeline = {}});
  desc.variants.back().pipeline.vertex_buffers = {
      {.stride = 24,
       .step_mode = GRANIT_VERTEX_STEP_MODE_VERTEX,
       .attributes = {{0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0},
                      {1, GRANIT_VERTEX_FORMAT_FLOAT32X3, 12}}}};
  desc.variants.back().pipeline.primitive.cull_mode = GRANIT_CULL_MODE_BACK;
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

void write_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) {
  for (std::uint32_t index = 0; index < 4; ++index) {
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
  }
}

void refresh_hash(std::span<std::byte> bytes) {
  const auto hash = calculate_material_archive_content_hash(bytes);
  std::ranges::copy(hash, bytes.begin() + 64);
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
  CHECK(read_u32(section(archive_section_type::material_metadata), 4) == 3);
  CHECK(read_u32(section(archive_section_type::feature_definitions), 0) == 1);
  CHECK(read_u32(section(archive_section_type::pass_definitions), 0) == 1);
  CHECK(read_u32(section(archive_section_type::variant_records), 0) == 1);
  CHECK(read_u32(section(archive_section_type::shader_records), 0) == 2);
  CHECK(read_u32(section(archive_section_type::pipeline_states), 0) == 1);
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

TEST_CASE("材质归档往返保留 Pipeline 状态") {
  auto package = make_package(false);
  material_package_desc desc;
  desc.metadata.constant_buffer_size = package.metadata().constant_buffer_size();
  for (const auto& parameter : package.metadata().parameters()) {
    desc.metadata.parameters.push_back(parameter);
  }
  for (const auto& variant : package.variants()) {
    desc.variants.push_back({.pass = variant.pass,
                             .features = variant.features,
                             .shaders = variant.shaders,
                             .pipeline = variant.pipeline});
  }
  REQUIRE(material_package::build(std::move(desc), package) == package_error::none);
  std::vector<std::byte> bytes;
  REQUIRE(encode_material_package_archive(package, bytes) == archive_error::none);
  material_package decoded;
  REQUIRE(decode_material_package_archive(bytes, decoded) == archive_error::none);
  REQUIRE(decoded.variants().size() == 1);
  CHECK(decoded.variants().front().pipeline == package.variants().front().pipeline);
}

TEST_CASE("材质包编码解码再编码保持逐字节一致") {
  const auto source = make_package(false);
  std::vector<std::byte> encoded;
  REQUIRE(encode_material_package_archive(source, encoded) == archive_error::none);

  material_package decoded;
  REQUIRE(decode_material_package_archive(encoded, decoded) == archive_error::none);
  REQUIRE(decoded.metadata().parameters().size() == 3);
  const auto* base_color = decoded.metadata().find(make_parameter_id("base_color"));
  REQUIRE(base_color != nullptr);
  CHECK(base_color->default_value == std::vector<std::byte>(16, std::byte{0x01}));
  const auto* albedo = decoded.metadata().find(make_parameter_id("albedo"));
  REQUIRE(albedo != nullptr);
  CHECK(albedo->type == parameter_type::texture_view);
  CHECK(albedo->binding == 1);
  REQUIRE(decoded.variants().size() == 1);
  CHECK(decoded.variants().front().features.size() == 1);
  CHECK(decoded.variants().front().shaders.size() == 2);

  std::vector<std::byte> reencoded;
  REQUIRE(encode_material_package_archive(decoded, reencoded) == archive_error::none);
  CHECK(reencoded == encoded);
}

TEST_CASE("材质包解码拒绝哈希正确但变体键被篡改的归档") {
  const auto source = make_package(false);
  std::vector<std::byte> bytes;
  REQUIRE(encode_material_package_archive(source, bytes) == archive_error::none);
  material_archive_layout layout;
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);
  const auto variant = std::ranges::find(
      layout.sections, static_cast<std::uint32_t>(archive_section_type::variant_records),
      &material_archive_section::type);
  REQUIRE(variant != layout.sections.end());
  bytes[variant->offset + 24] ^= std::byte{1};
  refresh_hash(bytes);

  material_package decoded;
  CHECK(decode_material_package_archive(bytes, decoded) == archive_error::invalid_semantic_data);
}

TEST_CASE("材质包解码拒绝非法 UTF-8 字符串") {
  const auto source = make_package(false);
  std::vector<std::byte> bytes;
  REQUIRE(encode_material_package_archive(source, bytes) == archive_error::none);
  material_archive_layout layout;
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);
  const auto strings = std::ranges::find(
      layout.sections, static_cast<std::uint32_t>(archive_section_type::string_table),
      &material_archive_section::type);
  REQUIRE(strings != layout.sections.end());
  bytes[strings->offset] = std::byte{0xc0};
  refresh_hash(bytes);

  material_package decoded;
  CHECK(decode_material_package_archive(bytes, decoded) == archive_error::invalid_semantic_data);
}

TEST_CASE("材质包解码在分配前拒绝超限记录数量") {
  const auto source = make_package(false);
  std::vector<std::byte> bytes;
  REQUIRE(encode_material_package_archive(source, bytes) == archive_error::none);
  material_archive_layout layout;
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);
  const auto metadata = std::ranges::find(
      layout.sections, static_cast<std::uint32_t>(archive_section_type::material_metadata),
      &material_archive_section::type);
  REQUIRE(metadata != layout.sections.end());
  write_u32(bytes, metadata->offset + 4, UINT32_MAX);
  refresh_hash(bytes);

  material_package decoded;
  CHECK(decode_material_package_archive(bytes, decoded) == archive_error::invalid_semantic_data);
}

TEST_CASE("材质包解码拒绝 Pipeline 状态保留字段被篡改") {
  const auto source = make_package(false);
  std::vector<std::byte> bytes;
  REQUIRE(encode_material_package_archive(source, bytes) == archive_error::none);
  material_archive_layout layout;
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);
  const auto pipeline = std::ranges::find(
      layout.sections, static_cast<std::uint32_t>(archive_section_type::pipeline_states),
      &material_archive_section::type);
  REQUIRE(pipeline != layout.sections.end());
  write_u32(bytes, pipeline->offset + 24 + 72, 1);
  refresh_hash(bytes);

  material_package decoded;
  CHECK(decode_material_package_archive(bytes, decoded) == archive_error::invalid_semantic_data);
}

TEST_CASE("材质包调试 JSON 使用稳定字段与固定宽度标识") {
  const auto source = make_package(false);
  std::vector<std::byte> bytes;
  REQUIRE(encode_material_package_archive(source, bytes) == archive_error::none);
  std::string json;
  REQUIRE(export_material_archive_debug_json(bytes, json) == archive_error::none);
  CHECK(json.starts_with("{\n  \"format\""));
  CHECK(json.find("\"magic\": \"GRMAT\"") != std::string::npos);
  CHECK(json.find("\"binding_model\": \"bind_group\"") != std::string::npos);
  CHECK(json.find("\"name\": \"base_color\"") != std::string::npos);
  CHECK(json.find("\"type\": \"texture_view\"") != std::string::npos);
  CHECK(json.find("\"stage\": \"vertex\"") != std::string::npos);
  CHECK(json.find("\"asset_id\":") != std::string::npos);
  CHECK(json.find("\"pipeline\": {\"vertex_buffers\"") != std::string::npos);
  CHECK(json.find("\"format\": \"float32x3\"") != std::string::npos);
  CHECK(json.ends_with("\n}\n"));
}
