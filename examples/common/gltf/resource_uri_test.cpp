// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include "gltf/fixtures/minimal_scene_glb.h"
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
  explicit memory_resolver(std::vector<std::byte> data, std::string path = "scene.bin")
      : data_(std::move(data)), path_(std::move(path)) {}

  bool resolve(std::string_view path, std::vector<std::byte>& output) const override {
    if (path != path_)
      return false;
    output = data_;
    return true;
  }

private:
  std::vector<std::byte> data_;
  std::string path_;
};

template <typename Value> void append(std::vector<std::byte>& output, const Value& value) {
  const auto offset = output.size();
  output.resize(offset + sizeof(value));
  std::memcpy(output.data() + offset, &value, sizeof(value));
}

void append_u32(std::vector<std::byte>& output, std::uint32_t value) { append(output, value); }

std::vector<std::byte> make_glb(std::string json, std::vector<std::byte> binary) {
  while (json.size() % 4 != 0)
    json.push_back(' ');
  while (binary.size() % 4 != 0)
    binary.push_back(std::byte{});
  const auto total_size = 12U + 8U + static_cast<std::uint32_t>(json.size()) + 8U +
                          static_cast<std::uint32_t>(binary.size());
  std::vector<std::byte> output;
  append_u32(output, 0x46546c67U);
  append_u32(output, 2U);
  append_u32(output, total_size);
  append_u32(output, static_cast<std::uint32_t>(json.size()));
  append_u32(output, 0x4e4f534aU);
  const auto json_offset = output.size();
  output.resize(json_offset + json.size());
  std::memcpy(output.data() + json_offset, json.data(), json.size());
  append_u32(output, static_cast<std::uint32_t>(binary.size()));
  append_u32(output, 0x004e4942U);
  output.insert(output.end(), binary.begin(), binary.end());
  return output;
}

} // namespace

static_assert(!std::is_copy_constructible_v<granit::example::gltf::resource_resolver>);

TEST_CASE("glTF 资源 URI 仅接受受控相对路径", "[example][gltf][uri]") {
  std::string normalized = "unchanged";
  REQUIRE(granit::example::gltf::normalize_resource_uri("textures/./base_color.png", normalized));
  CHECK(normalized == "textures/base_color.png");

  for (const std::string_view invalid :
       {"", "../secret.bin", "textures/../../secret.bin", "/absolute.bin", "C:/absolute.bin",
        "https://host/a.bin", "data:application/octet-stream;base64,AA==", "a\\b.bin",
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
    "nodes":[{"name":"root","translation":[1,2,3],"children":[1]},
             {"name":"child","matrix":[1,0,0,0,0,1,0,0,0,0,1,0,4,5,6,1]}]
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
  CHECK(scene.nodes[1].local_transform[12] == 4.0F);
  CHECK(scene.nodes[1].world_transform[12] == 5.0F);
  CHECK(scene.nodes[1].world_transform[13] == 7.0F);
  CHECK(scene.nodes[1].world_transform[14] == 9.0F);
}

TEST_CASE("glTF Loader 读取仓库固定 GLB Fixture", "[example][gltf][loader][fixture]") {
  const auto& fixture = granit::example::gltf::fixtures::minimal_scene_glb;
  const std::span document{reinterpret_cast<const std::byte*>(fixture), sizeof(fixture)};
  granit::example::gltf::scene scene;
  const auto result = granit::example::gltf::load(document, nullptr, scene);
  REQUIRE(result);
  REQUIRE(scene.nodes.size() == 1);
  CHECK(scene.nodes[0].name == "fixture");
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
      {"buffer":0,"byteOffset":0,"byteLength":72,"byteStride":24},
      {"buffer":0,"byteOffset":72,"byteLength":6}
    ],
    "accessors":[
      {"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3"},
      {"bufferView":0,"byteOffset":12,"componentType":5126,"count":3,"type":"VEC3"},
      {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
    ],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"indices":2}]}],
    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0
  })";
  std::vector<std::byte> buffer;
  for (const float value : {-1.0F, -2.0F, 0.0F, 0.0F, 0.0F, 1.0F, 3.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                            1.0F, 0.0F, 4.0F, -1.0F, 0.0F, 0.0F, 1.0F})
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

TEST_CASE("glTF Loader 读取 GLB 内嵌 BIN", "[example][gltf][loader][glb]") {
  std::vector<std::byte> binary;
  for (const float value : {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                            0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F})
    append(binary, value);
  const auto glb = make_glb(
      R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":72}],"bufferViews":[{"buffer":0,"byteLength":72,"byteStride":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":0,"byteOffset":12,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})",
      std::move(binary));

  granit::example::gltf::scene scene;
  const auto result = granit::example::gltf::load(glb, nullptr, scene);
  INFO(result.diagnostic);
  REQUIRE(result);
  REQUIRE(scene.meshes.size() == 1);
  CHECK(scene.meshes[0].primitives[0].indices == std::vector<std::uint32_t>{0, 1, 2});
}

TEST_CASE("glTF Loader 解码 PBR Material 图片与 Sampler", "[example][gltf][loader]") {
  constexpr std::string_view document = R"({
    "asset":{"version":"2.0"},
    "images":[{"name":"pixel","uri":"pixel.png"}],
    "samplers":[{"magFilter":9729,"minFilter":9987,"wrapS":10497,"wrapT":33071}],
    "textures":[{"source":0,"sampler":0}],
    "materials":[{"name":"paint","pbrMetallicRoughness":{
      "baseColorFactor":[0.5,0.6,0.7,1.0],"metallicFactor":0.25,"roughnessFactor":0.75,
      "baseColorTexture":{"index":0}
    }}]
  })";
  const std::uint8_t png[] = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
      0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x04, 0x00, 0x00,
      0x00, 0xb5, 0x1c, 0x0c, 0x02, 0x00, 0x00, 0x00, 0x0b, 0x49, 0x44, 0x41, 0x54, 0x78,
      0xda, 0x63, 0x64, 0xf8, 0x0f, 0x00, 0x01, 0x05, 0x01, 0x01, 0x27, 0x18, 0xe3, 0x66,
      0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
  std::vector<std::byte> encoded(sizeof(png));
  std::memcpy(encoded.data(), png, sizeof(png));
  const memory_resolver resolver(std::move(encoded), "pixel.png");

  granit::example::gltf::scene scene;
  const auto result = granit::example::gltf::load(bytes(document), &resolver, scene);
  REQUIRE(result);
  REQUIRE(scene.images.size() == 1);
  REQUIRE(scene.images[0].mips.size() == 1);
  CHECK(scene.images[0].name == "pixel");
  CHECK(scene.images[0].mips[0].width == 1);
  CHECK(scene.images[0].mips[0].height == 1);
  CHECK(scene.images[0].rgba8_pixels.size() == 4);
  REQUIRE(scene.materials.size() == 1);
  CHECK(scene.materials[0].base_color == granit::math::float4{0.5F, 0.6F, 0.7F, 1.0F});
  CHECK(scene.materials[0].metallic == 0.25F);
  CHECK(scene.materials[0].roughness == 0.75F);
  CHECK(scene.materials[0].base_color_texture.image == 0);
  CHECK(scene.materials[0].base_color_texture.sampler == 0);
  REQUIRE(scene.samplers.size() == 1);
  CHECK(scene.samplers[0].wrap_v == 33071);
}

TEST_CASE("glTF Loader 区分资源和数据错误", "[example][gltf][loader][error]") {
  SECTION("拒绝不安全 URI") {
    constexpr std::string_view document =
        R"({"asset":{"version":"2.0"},"buffers":[{"uri":"../scene.bin","byteLength":1}]})";
    const memory_resolver resolver({std::byte{1}});
    granit::example::gltf::scene scene;
    const auto result = granit::example::gltf::load(bytes(document), &resolver, scene);
    CHECK(result.error == granit::example::gltf::load_error::invalid_resource_uri);
  }

  SECTION("区分资源缺失和截断") {
    constexpr std::string_view document =
        R"({"asset":{"version":"2.0"},"buffers":[{"uri":"scene.bin","byteLength":4}]})";
    granit::example::gltf::scene scene;
    CHECK(granit::example::gltf::load(bytes(document), nullptr, scene).error ==
          granit::example::gltf::load_error::missing_resource);
    const memory_resolver resolver({std::byte{1}});
    CHECK(granit::example::gltf::load(bytes(document), &resolver, scene).error ==
          granit::example::gltf::load_error::truncated_data);
  }

  SECTION("区分 Accessor 越界") {
    constexpr std::string_view document = R"({
      "asset":{"version":"2.0"},"buffers":[{"uri":"scene.bin","byteLength":12}],
      "bufferViews":[{"buffer":0,"byteLength":12}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"VEC3"}]
    })";
    const memory_resolver resolver(std::vector<std::byte>(12));
    granit::example::gltf::scene scene;
    CHECK(granit::example::gltf::load(bytes(document), &resolver, scene).error ==
          granit::example::gltf::load_error::accessor_out_of_bounds);
  }

  SECTION("区分图片解码失败") {
    constexpr std::string_view document =
        R"({"asset":{"version":"2.0"},"images":[{"uri":"bad.png"}]})";
    const memory_resolver resolver({std::byte{1}, std::byte{2}}, "bad.png");
    granit::example::gltf::scene scene;
    CHECK(granit::example::gltf::load(bytes(document), &resolver, scene).error ==
          granit::example::gltf::load_error::image_decode_failed);
  }

  SECTION("拒绝未支持的材质扩展") {
    constexpr std::string_view document = R"({
      "asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_clearcoat"],
      "materials":[{"extensions":{"KHR_materials_clearcoat":{"clearcoatFactor":1.0}}}]
    })";
    granit::example::gltf::scene scene;
    CHECK(granit::example::gltf::load(bytes(document), nullptr, scene).error ==
          granit::example::gltf::load_error::unsupported_feature);
  }

  SECTION("可选 Transmission 使用核心 PBR 回退") {
    constexpr std::string_view document = R"({
      "asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_transmission"],
      "materials":[{"extensions":{"KHR_materials_transmission":{"transmissionFactor":1.0}},
                    "pbrMetallicRoughness":{"metallicFactor":0.2}}]
    })";
    granit::example::gltf::scene scene;
    const auto result = granit::example::gltf::load(bytes(document), nullptr, scene);
    REQUIRE(result);
    REQUIRE(scene.materials.size() == 1);
    CHECK(scene.materials.front().metallic == Catch::Approx(0.2F));
  }

  SECTION("拒绝必需 Transmission") {
    constexpr std::string_view document = R"({
      "asset":{"version":"2.0"},
      "extensionsUsed":["KHR_materials_transmission"],
      "extensionsRequired":["KHR_materials_transmission"],
      "materials":[{"extensions":{"KHR_materials_transmission":{"transmissionFactor":1.0}}}]
    })";
    granit::example::gltf::scene scene;
    CHECK(granit::example::gltf::load(bytes(document), nullptr, scene).error ==
          granit::example::gltf::load_error::unsupported_feature);
  }
}
