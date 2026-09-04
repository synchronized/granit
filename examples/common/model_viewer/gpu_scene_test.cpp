// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include "model_viewer/gpu_scene.h"

#include <granit/renderer/renderer.hpp>

#include <cmath>
#include <limits>

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
  REQUIRE(plan.renderables.size() == plan.draws.size());
  CHECK(plan.renderables[0].payload == plan.draws[0].payload);
  CHECK(plan.renderables[0].object_id == 1);
  CHECK(plan.renderables[1].object_id == 2);
  CHECK(plan.renderables[2].object_id == 3);
  CHECK(plan.renderables[0].layer_mask == std::numeric_limits<std::uint64_t>::max());
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

TEST_CASE("GPU Scene 计划计算世界 Bounds 与法线矩阵", "[example][model-viewer]") {
  granit::example::gltf::scene source;
  auto& primitive = source.meshes.emplace_back().primitives.emplace_back();
  primitive.local_bounds = {.minimum = {-1, -1, -1}, .maximum = {1, 1, 1}, .valid = true};
  auto& node = source.nodes.emplace_back();
  node.mesh = 0;
  node.world_transform = {{2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 4, 5, 6, 1}};

  granit::example::model_viewer::gpu_scene_plan plan;
  REQUIRE(granit::example::model_viewer::build_gpu_scene_plan(source, plan) ==
          granit::example::model_viewer::gpu_scene_plan_error::none);
  REQUIRE(plan.draws.size() == 1);
  CHECK(plan.draws.front().model == node.world_transform);
  CHECK(plan.draws.front().normal_matrix[0] == Catch::Approx(0.5F));
  CHECK(plan.draws.front().normal_matrix[5] == Catch::Approx(0.5F));
  CHECK(plan.draws.front().normal_matrix[10] == Catch::Approx(0.5F));
  CHECK(plan.draws.front().bounds_center == granit::math::float3{4, 5, 6});
  CHECK(plan.draws.front().bounds_radius == Catch::Approx(std::sqrt(12.0F)));
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
  if (renderer_result.failed())
    SKIP("当前环境没有可用 Renderer");

  granit::example::gltf::scene source;
  auto& primitive = source.meshes.emplace_back().primitives.emplace_back();
  primitive.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  primitive.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  primitive.indices = {0, 1, 2};
  primitive.material = 0;
  source.materials.emplace_back();
  source.nodes.emplace_back().mesh = 0;

  granit::example::model_viewer::gpu_scene scene;
  REQUIRE(scene.initialize(renderer.native_handle(), source) == granit::result::success);
  CHECK(scene.valid());
  CHECK(scene.meshes().size() == 1);
  CHECK(scene.materials().size() == 2);
  REQUIRE(scene.draw_bindings().size() == 1);
  CHECK(scene.draw_bindings().front().payload == 1);
  CHECK(scene.draw_bindings().front().mesh == scene.meshes().front().native_handle());
  CHECK(scene.draw_bindings().front().material == scene.materials().front().native_handle());
  REQUIRE(scene.renderables().size() == 1);
  CHECK(scene.renderables().front().payload == scene.draw_bindings().front().payload);
  const auto original_mesh = scene.meshes().front().native_handle();
  CHECK(scene.initialize(renderer.native_handle(), source, 0.0F) ==
        granit::result::invalid_argument);
  CHECK(scene.meshes().front().native_handle() == original_mesh);

  const granit::example::model_viewer::material_factor_edit edit{
      .base_color = {0.2F, 0.3F, 0.4F, 1.0F},
      .metallic = 0.5F,
      .roughness = 0.6F,
      .normal_scale = 0.7F,
      .occlusion_strength = 0.8F,
      .emissive = {1.0F, 2.0F, 3.0F}};
  REQUIRE(scene.update_material_factors(source, 0, edit) == granit::result::success);
  CHECK(source.materials[0].base_color == edit.base_color);
  CHECK(source.materials[0].metallic == edit.metallic);
  auto invalid_edit = edit;
  invalid_edit.roughness = std::numeric_limits<float>::infinity();
  CHECK(scene.update_material_factors(source, 0, invalid_edit) == granit::result::invalid_argument);
  CHECK(source.materials[0].roughness == edit.roughness);

  const granit_scene_view view{.view = granit::math::identity_matrix4,
                               .projection = granit::math::identity_matrix4,
                               .view_projection = granit::math::identity_matrix4,
                               .camera_position = {0, 0, 2},
                               .viewport_x = 0,
                               .viewport_y = 0,
                               .viewport_width = 640,
                               .viewport_height = 480,
                               .layer_mask = std::numeric_limits<std::uint64_t>::max()};
  granit::scene_snapshot snapshot;
  REQUIRE(scene.create_snapshot(std::span{&view, 1}, {}, {}, {}, snapshot) ==
          granit::result::success);
  CHECK(snapshot.valid());

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
  granit_texture_view view = 1;
  granit_sampler sampler = 1;
  CHECK(scene.texture_binding({}, false, view, sampler) == granit::result::invalid_handle);
  CHECK(view == GRANIT_NULL_HANDLE);
  CHECK(sampler == GRANIT_NULL_HANDLE);
}

TEST_CASE("材质 GPU 更新失败时保留 CPU Factor", "[example][model-viewer][transaction]") {
  granit::example::model_viewer::gpu_scene gpu;
  granit::example::gltf::scene source;
  source.materials.emplace_back().roughness = 0.4F;
  granit::example::model_viewer::material_factor_edit edit;
  edit.roughness = 0.8F;
  CHECK(gpu.update_material_factors(source, 0, edit) == granit::result::invalid_handle);
  CHECK(source.materials.front().roughness == 0.4F);
}
