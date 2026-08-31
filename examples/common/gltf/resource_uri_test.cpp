// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include "gltf/loader.h"
#include "gltf/resource_uri.h"

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::example::gltf::resource_resolver>);

TEST_CASE("glTF 资源 URI 仅接受受控相对路径", "[example][gltf][uri]") {
  std::string normalized = "unchanged";
  REQUIRE(granit::example::gltf::normalize_resource_uri(
      "textures/./base_color.png", normalized));
  CHECK(normalized == "textures/base_color.png");

  for (const std::string_view invalid : {"", "../secret.bin", "textures/../../secret.bin",
                                         "/absolute.bin", "C:/absolute.bin", "https://host/a.bin",
                                         "data:application/octet-stream;base64,AA==", "a\\b.bin",
                                         "a%2f..%2fsecret.bin", "a.bin?x=1", "a.bin#fragment"}) {
    normalized = "unchanged";
    CHECK_FALSE(granit::example::gltf::normalize_resource_uri(invalid, normalized));
    CHECK(normalized == "unchanged");
  }
}

TEST_CASE("glTF CPU Scene 使用自有存储", "[example][gltf][scene]") {
  granit::example::gltf::scene scene;
  scene.meshes.emplace_back().primitives.emplace_back().positions.push_back({1, 2, 3});
  scene.nodes.emplace_back().mesh = 0;
  scene.roots.push_back(0);

  REQUIRE(scene.meshes.size() == 1);
  CHECK(scene.meshes.front().primitives.front().positions.front().z == 3.0F);
  CHECK(scene.nodes.front().mesh == 0);
  CHECK(scene.roots.front() == 0);
}
