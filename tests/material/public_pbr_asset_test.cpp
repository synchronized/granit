// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/shader_asset.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
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

struct loaded_asset {
  std::vector<std::byte> manifest;
  std::vector<std::byte> spirv;
  std::string wgsl;
  granit::tools::shader_asset_view view;
};

loaded_asset load_asset(std::string_view name) {
  const auto base = std::filesystem::path{GRANIT_PBR_ASSET_DIR} / name;
  loaded_asset result;
  result.manifest = read_binary(base);
  result.spirv = read_binary(base.string() + ".spv");
  const auto wgsl_bytes = read_binary(base.string() + ".wgsl");
  result.wgsl.assign(reinterpret_cast<const char*>(wgsl_bytes.data()), wgsl_bytes.size());
  REQUIRE_FALSE(result.manifest.empty());
  REQUIRE_FALSE(result.spirv.empty());
  REQUIRE_FALSE(result.wgsl.empty());
  REQUIRE(granit::tools::decode_shader_asset(result.manifest, result.view) ==
          granit::tools::shader_asset_error::success);
  REQUIRE(granit::tools::validate_shader_asset_payloads(result.view, result.wgsl, result.spirv) ==
          granit::tools::shader_asset_error::success);
  return result;
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
  const auto asset = load_asset("pbr_standard.vert.grshader");
  CHECK(asset.view.stage == 1);
  CHECK(asset.view.entry_point == "vertex_main");
  require_binding(asset.view.reflection_json, 0, 0, "uniform_buffer", 128);
  require_binding(asset.view.reflection_json, 2, 0, "uniform_buffer", 144);
  for (std::uint32_t location = 0; location < 4; ++location)
    CHECK(asset.view.reflection_json.find("\"location\": " + std::to_string(location)) !=
          std::string_view::npos);
}

TEST_CASE("公共 PBR 片段资产固定材质和 IBL 契约") {
  const auto asset = load_asset("pbr_standard.frag.grshader");
  CHECK(asset.view.stage == 2);
  CHECK(asset.view.entry_point == "fragment_main");
  require_binding(asset.view.reflection_json, 0, 0, "uniform_buffer", 128);
  require_binding(asset.view.reflection_json, 1, 0, "uniform_buffer", 48);
  for (std::uint32_t binding = 1; binding <= 5; ++binding)
    require_binding(asset.view.reflection_json, 1, binding, "sampled_texture", 0);
  require_binding(asset.view.reflection_json, 1, 6, "sampler", 0);
  require_binding(asset.view.reflection_json, 3, 3, "uniform_buffer", 16);
  for (std::uint32_t binding = 4; binding <= 6; ++binding)
    require_binding(asset.view.reflection_json, 3, binding, "sampled_texture", 0);
  require_binding(asset.view.reflection_json, 3, 7, "sampler", 0);
}
