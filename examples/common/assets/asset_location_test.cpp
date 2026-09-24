// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_location.h"

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <string>

namespace assets = granit::example::assets;

TEST_CASE("资产位置解析同时支持 URL 和文件路径", "[example][assets][location]") {
  std::string output;
  CHECK(
      assets::resolve_asset_location("https://example.com/models/a.gltf?x=1", "mesh.bin", output));
  CHECK(output == "https://example.com/models/mesh.bin");

  CHECK(assets::resolve_asset_location("models/a.gltf", "textures/./base.png", output));
  CHECK(std::filesystem::path{output} == std::filesystem::path{"models"} / "textures" / "base.png");

  CHECK(assets::resolve_asset_location("https://example.com/models/a.gltf",
                                       "https://cdn.example.com/mesh.bin", output));
  CHECK(output == "https://cdn.example.com/mesh.bin");
}

TEST_CASE("资产位置解析拒绝逃逸与无效输入", "[example][assets][location]") {
  std::string output = "unchanged";
  for (const std::string_view invalid : {"", "../secret.bin", "textures/../../secret.bin",
                                         "a\\b.bin", "a.bin?x=1", "a.bin#fragment"}) {
    CHECK_FALSE(assets::resolve_asset_location("models/a.gltf", invalid, output));
    CHECK(output == "unchanged");
  }

  const std::string nul_base{"models/a.gltf\0ignored", 21};
  const std::string nul_relative{"mesh.bin\0ignored", 16};
  CHECK_FALSE(assets::resolve_asset_location(nul_base, "mesh.bin", output));
  CHECK_FALSE(assets::resolve_asset_location("models/a.gltf", nul_relative, output));
  CHECK(output == "unchanged");
}
