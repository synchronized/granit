// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include "gltf/loader.h"
#include "gltf/resource_uri.h"

#include <cstring>
#include <type_traits>

namespace {

std::span<const std::byte> bytes(std::string_view text) {
  return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

class memory_resolver final : public granit::example::gltf::resource_resolver {
public:
  explicit memory_resolver(std::vector<std::byte> data) : data_(std::move(data)) {}

  bool resolve(std::string_view path, std::vector<std::byte>& output) const override {
    if (path != "scene.bin")
      return false;
    output = data_;
    return true;
  }

private:
  std::vector<std::byte> data_;
};

template <typename Value>
void append(std::vector<std::byte>& output, const Value& value) {
  const auto offset = output.size();
  output.resize(offset + sizeof(value));
  std::memcpy(output.data() + offset, &value, sizeof(value));
}

} // namespace

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

TEST_CASE("glTF Loader 转换 Node 层级和 TRS", "[example][gltf][loader]") {
  constexpr std::string_view document = R"({
    "asset":{"version":"2.0"},
    "scene":0,
    "scenes":[{"nodes":[0]}],
    "nodes":[{"name":"root","translation":[1,2,3],"children":[1]},{"name":"child"}]
  })";
  granit::example::gltf::scene scene;
  const auto result = granit::example::gltf::load(bytes(document), nullptr, scene);
  REQUIRE(result);
  REQUIRE(scene.nodes.size() == 2);
  CHECK(scene.roots == std::vector<std::uint32_t>{0});
  CHECK(scene.nodes[0].children == std::vector<std::uint32_t>{1});
  CHECK(scene.nodes[1].parent == 0);
  CHECK(scene.nodes[0].world_transform[12] == 1.0F);
  CHECK(scene.nodes[0].world_transform[13] == 2.0F);
  CHECK(scene.nodes[0].world_transform[14] == 3.0F);
}

TEST_CASE("glTF Loader 失败时保持输出不变", "[example][gltf][loader]") {
  granit::example::gltf::scene scene;
  scene.nodes.emplace_back().name = "保留";
  const auto result = granit::example::gltf::load({}, nullptr, scene);
  CHECK_FALSE(result);
  CHECK(result.error == granit::example::gltf::load_error::truncated_data);
  REQUIRE(scene.nodes.size() == 1);
  CHECK(scene.nodes.front().name == "保留");
}

TEST_CASE("glTF Loader 读取 Primitive、索引和 AABB", "[example][gltf][loader]") {
  constexpr std::string_view document = R"({
    "asset":{"version":"2.0"},
    "buffers":[{"uri":"scene.bin","byteLength":78}],
    "bufferViews":[
      {"buffer":0,"byteOffset":0,"byteLength":36},
      {"buffer":0,"byteOffset":36,"byteLength":36},
      {"buffer":0,"byteOffset":72,"byteLength":6}
    ],
    "accessors":[
      {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
      {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
      {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}
    ],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"indices":2}]}],
    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0
  })";
  std::vector<std::byte> buffer;
  for (const float value : {-1.0F, -2.0F, 0.0F, 3.0F, 0.0F, 1.0F, 0.0F, 4.0F, -1.0F})
    append(buffer, value);
  for (const float value : {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F})
    append(buffer, value);
  for (const std::uint16_t value : {0, 1, 2})
    append(buffer, value);

  const memory_resolver resolver(std::move(buffer));
  granit::example::gltf::scene scene;
  const auto result = granit::example::gltf::load(bytes(document), &resolver, scene);
  REQUIRE(result);
  REQUIRE(scene.meshes.size() == 1);
  const auto& primitive = scene.meshes.front().primitives.front();
  CHECK(primitive.indices == std::vector<std::uint32_t>{0, 1, 2});
  CHECK(primitive.positions.size() == 3);
  CHECK(primitive.normals.size() == 3);
  CHECK(primitive.local_bounds.minimum == granit::math::float3{-1, -2, -1});
  CHECK(primitive.local_bounds.maximum == granit::math::float3{3, 4, 1});
}
