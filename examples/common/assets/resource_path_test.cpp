// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/resource_path.h"

#include <catch2/catch_all.hpp>

TEST_CASE("资源路径只接受受控相对路径", "[example][assets][path]") {
  std::string normalized = "unchanged";
  REQUIRE(
      granit::example::assets::normalize_resource_path("textures/./base_color.png", normalized));
  CHECK(normalized == "textures/base_color.png");

  for (const std::string_view invalid :
       {"", "../secret.bin", "textures/../../secret.bin", "/absolute.bin", "C:/absolute.bin",
        "https://host/a.bin", "data:application/octet-stream;base64,AA==", "a\\b.bin",
        "a%2f..%2fsecret.bin", "a.bin?x=1", "a.bin#fragment"}) {
    normalized = "unchanged";
    CHECK_FALSE(granit::example::assets::normalize_resource_path(invalid, normalized));
    CHECK(normalized == "unchanged");
  }

  const std::string with_nul{"textures/base.png\0ignored", 25};
  CHECK_FALSE(granit::example::assets::normalize_resource_path(with_nul, normalized));
  CHECK(normalized == "unchanged");
}

TEST_CASE("资源 URI 相对主文档逻辑路径解析", "[example][assets][path]") {
  namespace assets = granit::example::assets;
  std::string output;
  REQUIRE(assets::resolve_resource_path("models/Suzanne/scene.gltf", "textures/./base.png",
                                        output));
  CHECK(output == "models/Suzanne/textures/base.png");

  REQUIRE(assets::resolve_resource_path("scene.gltf", "mesh.bin", output));
  CHECK(output == "mesh.bin");
}

TEST_CASE("资源 URI 解析拒绝平台位置与路径逃逸", "[example][assets][path]") {
  namespace assets = granit::example::assets;
  std::string output{"unchanged"};
  for (const std::string_view invalid : {"", "../secret.bin", "textures/../../secret.bin",
                                         "a\\b.bin", "a.bin?x=1", "a.bin#fragment",
                                         "https://cdn.example.com/mesh.bin", "/mesh.bin"}) {
    CHECK_FALSE(assets::resolve_resource_path("models/scene.gltf", invalid, output));
    CHECK(output == "unchanged");
  }
  CHECK_FALSE(assets::resolve_resource_path("https://example.com/scene.gltf", "mesh.bin",
                                            output));
  CHECK(output == "unchanged");
}
