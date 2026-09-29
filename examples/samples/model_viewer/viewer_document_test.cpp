// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "viewer_document.h"

#include <catch2/catch_all.hpp>

using namespace granit::example;

namespace {

gltf::scene make_scene() {
  gltf::scene scene;
  auto& primitive = scene.meshes.emplace_back().primitives.emplace_back();
  primitive.positions = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}};
  primitive.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  primitive.indices = {0, 1, 2};
  primitive.material = 0;
  primitive.local_bounds = {.minimum = {-1, -1, 0}, .maximum = {1, 1, 0}, .valid = true};
  scene.materials.emplace_back();
  scene.nodes.emplace_back().mesh = 0;
  return scene;
}

} // namespace

TEST_CASE("查看器文档生成与 GPU 资源无关的帧数据", "[example][model-viewer][document]") {
  auto scene = make_scene();
  gltf_rendering::scene_plan plan;
  REQUIRE(gltf_rendering::build_scene_plan(scene, plan) == gltf_rendering::scene_plan_error::none);

  model_viewer::viewer_document document;
  document.assign(std::move(scene));
  model_viewer::viewer_document_frame frame;
  model_viewer::viewer_document_update zero_sized;
  zero_sized.height = 480;
  CHECK(document.update(zero_sized, plan, frame) == granit::result::not_ready);

  model_viewer::viewer_document_update input;
  input.width = 640;
  input.height = 480;
  input.performance =
      model_viewer::performance_sample{.frames_per_second = 60.0F, .cpu_frame_ms = 2.0F};
  REQUIRE(document.update(input, plan, frame).ok());
  CHECK(frame.view.viewport_width == 640.0F);
  CHECK(frame.view.viewport_height == 480.0F);
  CHECK(frame.clear_color.red == Catch::Approx(0.025F));
  CHECK(frame.clear_color.green == Catch::Approx(0.04F));
  CHECK(frame.clear_color.blue == Catch::Approx(0.065F));
  CHECK(frame.environment_intensity == Catch::Approx(0.12F));
  CHECK(document.performance().size() == 1);
}

TEST_CASE("查看器文档统一更新 CPU 材质", "[example][model-viewer][document]") {
  auto scene = make_scene();
  model_viewer::viewer_document document;
  document.assign(std::move(scene));

  gltf_rendering::material_factor_update update;
  update.base_color = {0.1F, 0.2F, 0.3F, 0.4F};
  update.metallic = 0.5F;
  update.roughness = 0.6F;
  REQUIRE(document.update_material(0, update).ok());
  CHECK(document.scene().materials[0].base_color == update.base_color);
  CHECK(document.scene().materials[0].metallic == update.metallic);
  CHECK(document.scene().materials[0].roughness == update.roughness);
  CHECK(document.update_material(1, update) == granit::result::invalid_argument);
}
