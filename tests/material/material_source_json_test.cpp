// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_source_json.h"

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <string_view>

namespace {

constexpr std::string_view source = R"({
  "format_version": 2,
  "target_environment": "vulkan1.3",
  "binding_model": "bind_group",
  "material": {
    "constant_buffer_size": 16,
    "parameters": [
      {"name": "base_color", "type": "float4", "offset": 0,
       "default_bytes": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 63]},
      {"name": "albedo", "type": "texture_view", "binding": 1}
    ]
  },
  "variants": [{
    "pass": "opaque",
    "features": [{"name": "normal_map", "value": 1}],
    "shaders": [
      {"stage": "vertex", "entry_point": "main", "spirv": "minimal.vert.spv"},
      {"stage": "fragment", "entry_point": "main", "spirv": "minimal.frag.spv"}
    ]
  }]
})";

} // namespace

TEST_CASE("材质源 JSON 构建内存包并解析相对 SPIR-V 路径") {
  granit::material::material_package package;
  REQUIRE(granit::material::parse_material_source_json(
              source, std::filesystem::path{GRANIT_TEST_ASSET_DIR}, package) ==
          granit::material::source_json_error::none);
  CHECK(package.metadata().constant_buffer_size() == 16);
  CHECK(package.metadata().parameters().size() == 2);
  REQUIRE(package.variants().size() == 1);
  CHECK(package.variants().front().shaders.size() == 2);
}

TEST_CASE("材质源 JSON 拒绝不支持的绑定模型") {
  std::string invalid{source};
  invalid.replace(invalid.find("bind_group"), std::string_view{"bind_group"}.size(), "bindless");
  granit::material::material_package package;
  CHECK(granit::material::parse_material_source_json(
            invalid, std::filesystem::path{GRANIT_TEST_ASSET_DIR}, package) ==
        granit::material::source_json_error::unsupported_value);
}

TEST_CASE("材质源 JSON 拒绝缺失的 SPIR-V") {
  std::string invalid{source};
  invalid.replace(invalid.find("minimal.vert.spv"), std::string_view{"minimal.vert.spv"}.size(),
                  "missing.spv");
  granit::material::material_package package;
  CHECK(granit::material::parse_material_source_json(
            invalid, std::filesystem::path{GRANIT_TEST_ASSET_DIR}, package) ==
        granit::material::source_json_error::referenced_file_error);
}

TEST_CASE("材质源 JSON 拒绝超过限制的嵌套深度") {
  std::string deeply_nested(granit::material::material_source_json_max_depth + 2, '[');
  deeply_nested += "null";
  deeply_nested.append(granit::material::material_source_json_max_depth + 2, ']');
  granit::material::material_package package;
  CHECK(granit::material::parse_material_source_json(deeply_nested, {}, package) ==
        granit::material::source_json_error::invalid_json);
}
