// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include "model_viewer/gpu_scene.h"

TEST_CASE("GPU Scene 计划合并 Primitive 并记录字节 Offset", "[example][model-viewer]") {
  granit::example::gltf::scene source;
  auto& first = source.meshes.emplace_back().primitives.emplace_back();
  first.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  first.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  first.indices = {0, 1, 2};
  auto& second = source.meshes.emplace_back().primitives.emplace_back();
  second.positions = {{-1, 0, 0}, {0, -1, 0}, {0, 0, 0}};
  second.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  second.indices = {2, 1, 0};

  granit::example::model_viewer::gpu_scene_plan plan;
  REQUIRE(granit::example::model_viewer::build_gpu_scene_plan(source, plan) ==
          granit::example::model_viewer::gpu_scene_plan_error::none);
  REQUIRE(plan.primitives.size() == 2);
  CHECK(plan.primitives[0].vertex_offset == 0);
  CHECK(plan.primitives[0].index_offset == 0);
  CHECK(plan.primitives[1].vertex_offset ==
        3 * sizeof(granit::example::model_viewer::packed_vertex));
  CHECK(plan.primitives[1].index_offset == 3 * sizeof(std::uint32_t));
  CHECK(plan.vertices.size() == 6);
  CHECK(plan.indices == std::vector<std::uint32_t>{0, 1, 2, 2, 1, 0});
}

TEST_CASE("GPU Scene 计划按颜色空间拆分并去重纹理", "[example][model-viewer]") {
  granit::example::gltf::scene source;
  source.images.resize(2);
  auto& first = source.materials.emplace_back();
  first.base_color_texture.image = 0;
  first.normal_texture.image = 0;
  first.occlusion_texture.image = 1;
  auto& second = source.materials.emplace_back();
  second.emissive_texture.image = 0;
  second.metallic_roughness_texture.image = 1;

  granit::example::model_viewer::gpu_scene_plan plan;
  REQUIRE(granit::example::model_viewer::build_gpu_scene_plan(source, plan) ==
          granit::example::model_viewer::gpu_scene_plan_error::none);
  CHECK(plan.textures == std::vector<granit::example::model_viewer::texture_variant>{
                             {0, true}, {0, false}, {1, false}});
}

TEST_CASE("GPU Scene 计划失败时保持输出不变", "[example][model-viewer]") {
  granit::example::gltf::scene source;
  auto& primitive = source.meshes.emplace_back().primitives.emplace_back();
  primitive.positions.resize(2);
  primitive.normals.resize(1);
  granit::example::model_viewer::gpu_scene_plan plan;
  plan.indices.push_back(42);
  CHECK(granit::example::model_viewer::build_gpu_scene_plan(source, plan) ==
        granit::example::model_viewer::gpu_scene_plan_error::invalid_scene);
  CHECK(plan.indices == std::vector<std::uint32_t>{42});
}
