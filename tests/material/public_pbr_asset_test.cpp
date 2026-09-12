// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_archive.h"
#include "shader_format/shader_library.h"

#include <granit/pipeline/pbr_material.h>

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string_view>
#include <vector>

namespace {

std::vector<std::byte> read_binary(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> source{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(source.size());
  for (std::size_t index = 0; index < source.size(); ++index)
    result[index] = static_cast<std::byte>(source[index]);
  return result;
}

const granit::detail::shader_format::shader_library_shader&
find_shader(const granit::detail::shader_format::shader_library_view& library,
            granit::shader_stage stage) {
  const auto found = std::ranges::find(
      library.shaders, stage, &granit::detail::shader_format::shader_library_shader::stage);
  REQUIRE(found != library.shaders.end());
  return *found;
}

void require_binding(std::string_view reflection, std::uint32_t group, std::uint32_t binding,
                     std::string_view type, std::uint64_t minimum_size) {
  const auto prefix = std::string{"\"group\": "} + std::to_string(group) +
                      ", \"binding\": " + std::to_string(binding) + ", \"type\": \"" +
                      std::string{type} + "\"";
  const auto position = reflection.find(prefix);
  REQUIRE(position != std::string_view::npos);
  const auto end = reflection.find('}', position);
  REQUIRE(end != std::string_view::npos);
  const auto entry = reflection.substr(position, end - position);
  CHECK(entry.find("\"minimum_binding_size\": " + std::to_string(minimum_size)) !=
        std::string_view::npos);
}

} // namespace

TEST_CASE("公共 PBR 顶点资产固定 Frame Object 和顶点输入契约") {
  const auto bytes = read_binary(GRANIT_PBR_LIBRARY_ASSET);
  granit::detail::shader_format::shader_library_view library;
  REQUIRE(granit::detail::shader_format::decode_shader_library(bytes, library) ==
          granit::detail::shader_format::shader_library_error::success);
  const auto& shader = find_shader(library, granit::shader_stage::vertex);
  CHECK(shader.entry_point == "vertex_main");
  require_binding(shader.reflection_json, 0, 0, "uniform_buffer", 128);
  require_binding(shader.reflection_json, 2, 0, "uniform_buffer", 144);
  for (std::uint32_t location = 0; location < 4; ++location)
    CHECK(shader.reflection_json.find("\"location\": " + std::to_string(location)) !=
          std::string_view::npos);
}

TEST_CASE("公共 PBR 片段资产固定材质和 IBL 契约") {
  const auto bytes = read_binary(GRANIT_PBR_LIBRARY_ASSET);
  granit::detail::shader_format::shader_library_view library;
  REQUIRE(granit::detail::shader_format::decode_shader_library(bytes, library) ==
          granit::detail::shader_format::shader_library_error::success);
  const auto& shader = find_shader(library, granit::shader_stage::fragment);
  CHECK(shader.entry_point == "fragment_main");
  require_binding(shader.reflection_json, 0, 0, "uniform_buffer", 128);
  require_binding(shader.reflection_json, 1, 0, "uniform_buffer", 48);
  for (std::uint32_t binding = 1; binding <= 5; ++binding)
    require_binding(shader.reflection_json, 1, binding, "sampled_texture", 0);
  require_binding(shader.reflection_json, 1, 6, "sampler", 0);
  require_binding(shader.reflection_json, 3, 3, "uniform_buffer", 16);
  for (std::uint32_t binding = 4; binding <= 6; ++binding)
    require_binding(shader.reflection_json, 3, binding, "sampled_texture", 0);
  require_binding(shader.reflection_json, 3, 7, "sampler", 0);
}

TEST_CASE("公共 PBR 材质模板具有稳定 Schema 和内容身份") {
  const auto bytes = read_binary(GRANIT_PBR_MATERIAL_ASSET);
  granit::material::material_archive_layout layout;
  REQUIRE(granit::material::parse_material_archive_layout(bytes, layout) ==
          granit::material::archive_error::none);
  CHECK(GRANIT_PBR_MATERIAL_TEMPLATE_VERSION == 3);

  constexpr std::string_view hex = GRANIT_PBR_MATERIAL_CONTENT_HASH_HEX;
  REQUIRE(hex.size() == layout.header.content_hash.size() * 2);
  constexpr auto nibble = [](char value) {
    return static_cast<std::uint8_t>(value <= '9' ? value - '0' : value - 'a' + 10);
  };
  for (std::size_t index = 0; index < layout.header.content_hash.size(); ++index) {
    const auto expected =
        static_cast<std::uint8_t>((nibble(hex[index * 2]) << 4) | nibble(hex[index * 2 + 1]));
    CHECK(std::to_integer<std::uint8_t>(layout.header.content_hash[index]) == expected);
  }
}
