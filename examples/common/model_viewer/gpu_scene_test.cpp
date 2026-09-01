// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include "model_viewer/gpu_scene.h"

#include <granit/renderer/renderer.hpp>

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

TEST_CASE("GPU Scene 计划规范化并去重 Sampler", "[example][model-viewer]") {
  granit::example::gltf::scene source;
  source.samplers = {{.mag_filter = 9729, .min_filter = 9987, .wrap_u = 10497, .wrap_v = 33071},
                     {.mag_filter = 9729, .min_filter = 9987, .wrap_u = 10497, .wrap_v = 33071},
                     {.mag_filter = 9728, .min_filter = 9984, .wrap_u = 33648, .wrap_v = 10497}};
  granit::example::model_viewer::gpu_scene_plan plan;
  REQUIRE(granit::example::model_viewer::build_gpu_scene_plan(source, plan) ==
          granit::example::model_viewer::gpu_scene_plan_error::none);
  CHECK(plan.samplers.size() == 2);
  CHECK(plan.source_sampler_to_plan == std::vector<std::uint32_t>{0, 0, 1});
  CHECK(plan.samplers[0].address_v == granit::address_mode::clamp_to_edge);
  CHECK(plan.samplers[1].mag_filter == granit::filter::nearest);
  CHECK(plan.samplers[1].address_u == granit::address_mode::mirrored_repeat);
}

TEST_CASE("GPU Scene 计划按 Node 稳定展开 Primitive Payload", "[example][model-viewer]") {
  granit::example::gltf::scene source;
  source.materials.resize(2);
  source.meshes.resize(2);
  source.meshes[0].primitives.resize(2);
  source.meshes[0].primitives[0].material = 1;
  source.meshes[1].primitives.resize(1);
  source.meshes[1].primitives[0].material = 0;
  source.nodes.resize(3);
  source.nodes[0].mesh = 1;
  source.nodes[2].mesh = 0;

  granit::example::model_viewer::gpu_scene_plan plan;
  REQUIRE(granit::example::model_viewer::build_gpu_scene_plan(source, plan) ==
          granit::example::model_viewer::gpu_scene_plan_error::none);
  REQUIRE(plan.draws.size() == 3);
  CHECK(plan.draws[0].payload == 1);
  CHECK(plan.draws[0].primitive == 2);
  CHECK(plan.draws[0].material == 0);
  CHECK(plan.draws[0].node == 0);
  CHECK(plan.draws[1].payload == 2);
  CHECK(plan.draws[1].primitive == 0);
  CHECK(plan.draws[1].material == 1);
  CHECK(plan.draws[1].node == 2);
  CHECK(plan.draws[2].payload == 3);
  CHECK(plan.draws[2].primitive == 1);
}

TEST_CASE("GPU Scene 计划拒绝越界 Node 与 Material 引用", "[example][model-viewer]") {
  granit::example::gltf::scene source;
  source.meshes.resize(1);
  source.meshes.front().primitives.resize(1);
  source.meshes.front().primitives.front().material = 0;
  granit::example::model_viewer::gpu_scene_plan plan;
  CHECK(granit::example::model_viewer::build_gpu_scene_plan(source, plan) ==
        granit::example::model_viewer::gpu_scene_plan_error::invalid_scene);

  source.materials.resize(1);
  source.nodes.resize(1);
  source.nodes.front().mesh = 1;
  CHECK(granit::example::model_viewer::build_gpu_scene_plan(source, plan) ==
        granit::example::model_viewer::gpu_scene_plan_error::invalid_scene);
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

TEST_CASE("GPU Scene 事务式创建合并 Buffer 与 Mesh", "[example][model-viewer][gpu]") {
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "Model Viewer GPU Test"});
  if (granit::failed(renderer_result))
    SKIP("当前环境没有可用 Renderer");

  granit::example::gltf::scene source;
  auto& primitive = source.meshes.emplace_back().primitives.emplace_back();
  primitive.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  primitive.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  primitive.indices = {0, 1, 2};

  granit::example::model_viewer::gpu_scene scene;
  REQUIRE(scene.initialize(renderer.native_handle(), source) == granit::result::success);
  CHECK(scene.valid());
  CHECK(scene.meshes().size() == 1);

  granit::example::gltf::scene invalid_source;
  auto& invalid = invalid_source.meshes.emplace_back().primitives.emplace_back();
  invalid.positions.resize(2);
  invalid.normals.resize(1);
  CHECK(scene.initialize(renderer.native_handle(), invalid_source) ==
        granit::result::invalid_argument);
  CHECK(scene.valid());
  CHECK(scene.meshes().size() == 1);
  scene.reset();
  CHECK_FALSE(scene.valid());
}

TEST_CASE("GPU Scene 创建失败时保留原 Scene", "[example][model-viewer][transaction]") {
  granit::example::model_viewer::gpu_scene scene;
  granit::example::gltf::scene source;
  CHECK(scene.initialize(GRANIT_NULL_HANDLE, source) == granit::result::invalid_handle);
  CHECK_FALSE(scene.valid());
}
