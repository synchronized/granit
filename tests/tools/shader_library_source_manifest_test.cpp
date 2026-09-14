// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_tools/shader/library_source_manifest.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view valid_manifest = R"({
  "format_version": 1,
  "name": "pbr_standard",
  "target_profile": "portable",
  "target_backends": ["webgpu", "vulkan"],
  "shaders": [
    {
      "name": "standard.vertex",
      "source": "pbr/pbr_standard.hlsl",
      "stage": "vertex",
      "entry_point": "vertex_main"
    },
    {
      "name": "standard.fragment",
      "source": "pbr/pbr_standard.hlsl",
      "stage": "fragment",
      "entry_point": "fragment_main",
      "variants": [
        {
          "name": "textured",
          "defines": {
            "GRANIT_PBR_TEXTURE_MASK": "31",
            "GRANIT_PBR_IBL": "1"
          }
        },
        {
          "name": "untextured",
          "defines": {"GRANIT_PBR_TEXTURE_MASK": "0"}
        }
      ]
    }
  ]
})";

std::string replace_once(std::string text, std::string_view from, std::string_view to) {
  const auto position = text.find(from);
  REQUIRE(position != std::string::npos);
  text.replace(position, from.size(), to);
  return text;
}

} // namespace

TEST_CASE("Shader Library 源清单解析并规范化 Define") {
  granit::asset_tools::detail::shader_library_source_manifest manifest;
  REQUIRE(granit::asset_tools::detail::parse_shader_library_source_manifest(valid_manifest, manifest) ==
          granit::asset_tools::detail::shader_library_source_error::none);
  CHECK(manifest.name == "pbr_standard");
  CHECK(manifest.target_backends == granit::shader_backend::all);
  REQUIRE(manifest.shaders.size() == 2);
  CHECK(manifest.shaders[0].stage == granit::shader_stage::vertex);
  CHECK(manifest.shaders[1].stage == granit::shader_stage::fragment);
  REQUIRE(manifest.shaders[1].variants.size() == 2);
  REQUIRE(manifest.shaders[1].variants[0].defines.size() == 2);
  CHECK(manifest.shaders[1].variants[0].defines[0].name == "GRANIT_PBR_IBL");
  CHECK(manifest.shaders[1].variants[0].defines[1].name == "GRANIT_PBR_TEXTURE_MASK");
}

TEST_CASE("Shader Library 源清单拒绝无效语法和版本") {
  granit::asset_tools::detail::shader_library_source_manifest manifest;
  CHECK(granit::asset_tools::detail::parse_shader_library_source_manifest("{", manifest) ==
        granit::asset_tools::detail::shader_library_source_error::invalid_json);
  const auto unsupported =
      replace_once(std::string{valid_manifest}, "\"format_version\": 1", "\"format_version\": 2");
  CHECK(granit::asset_tools::detail::parse_shader_library_source_manifest(unsupported, manifest) ==
        granit::asset_tools::detail::shader_library_source_error::unsupported_version);
}

TEST_CASE("Shader Library 源清单严格校验作者输入") {
  const std::vector<std::string> invalid_manifests{
      replace_once(std::string{valid_manifest}, "\"name\": \"pbr_standard\"",
                   "\"name\": \"pbr_standard\", \"unknown\": 1"),
      replace_once(std::string{valid_manifest}, "pbr/pbr_standard.hlsl", "../pbr_standard.hlsl"),
      replace_once(std::string{valid_manifest}, "pbr/pbr_standard.hlsl", "pbr_standard.wgsl"),
      replace_once(std::string{valid_manifest}, "[\"webgpu\", \"vulkan\"]",
                   "[\"vulkan\", \"vulkan\"]"),
      replace_once(std::string{valid_manifest}, "\"variants\": [",
                   "\"unknown\": 1, \"variants\": ["),
      replace_once(std::string{valid_manifest}, "\"defines\": {\"GRANIT_PBR_TEXTURE_MASK\": \"0\"}",
                   "\"defines\": {}"),
  };
  for (const auto& json : invalid_manifests) {
    granit::asset_tools::detail::shader_library_source_manifest manifest;
    CHECK(granit::asset_tools::detail::parse_shader_library_source_manifest(json, manifest) ==
          granit::asset_tools::detail::shader_library_source_error::invalid_schema);
  }
}

TEST_CASE("Shader Library 索引编码按逻辑名称确定排序") {
  granit::asset_tools::detail::shader_library_index first{.library = "pbr_standard",
                                            .library_digest = {},
                                            .shaders = {{.name = "standard.fragment/textured",
                                                         .content_id = {},
                                                         .stage = granit::shader_stage::fragment,
                                                         .entry_point = "fragment_main"},
                                                        {.name = "standard.vertex",
                                                         .content_id = {},
                                                         .stage = granit::shader_stage::vertex,
                                                         .entry_point = "vertex_main"}}};
  auto second = first;
  std::ranges::reverse(second.shaders);
  std::string first_json;
  std::string second_json;
  REQUIRE(granit::asset_tools::detail::encode_shader_library_index_json(first, first_json) ==
          granit::asset_tools::detail::shader_library_source_error::none);
  REQUIRE(granit::asset_tools::detail::encode_shader_library_index_json(second, second_json) ==
          granit::asset_tools::detail::shader_library_source_error::none);
  CHECK(first_json == second_json);
  CHECK(first_json.find("standard.fragment/textured") < first_json.find("standard.vertex"));
  CHECK(first_json.ends_with("\n"));
  granit::asset_tools::detail::shader_library_index decoded;
  REQUIRE(granit::asset_tools::detail::parse_shader_library_index_json(first_json, decoded) ==
          granit::asset_tools::detail::shader_library_source_error::none);
  CHECK(decoded.library == first.library);
  CHECK(decoded.shaders.size() == first.shaders.size());
  CHECK(decoded.shaders.front().name == "standard.fragment/textured");
}

TEST_CASE("Shader Library 索引拒绝重复逻辑名称") {
  granit::asset_tools::detail::shader_library_index index{
      .library = "duplicate",
      .library_digest = {},
      .shaders = {{.name = "main", .content_id = {}, .entry_point = "main"},
                  {.name = "main", .content_id = {}, .entry_point = "main"}}};
  std::string json;
  CHECK(granit::asset_tools::detail::encode_shader_library_index_json(index, json) ==
        granit::asset_tools::detail::shader_library_source_error::invalid_argument);
}
