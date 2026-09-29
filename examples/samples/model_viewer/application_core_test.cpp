// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application_core.h"

#include <catch2/catch_all.hpp>

#include <limits>

using namespace granit::example::model_viewer;

TEST_CASE("模型查看器 Core 严格执行启动状态机", "[example][model-viewer][core]") {
  application_core core;
  CHECK(core.phase() == application_phase::platform_ready);
  CHECK(core.renderer_ready() == granit::result::invalid_argument);
  REQUIRE(core.begin_renderer().ok());
  REQUIRE(core.renderer_ready().ok());
  granit::example::gltf::scene scene;
  scene.nodes.resize(1);
  REQUIRE(core.accept_scene(std::move(scene)).ok());
  CHECK(core.phase() == application_phase::gpu_upload);
  granit::example::gltf::scene upload_scene;
  granit::example::gltf_rendering::scene_plan upload_plan;
  REQUIRE(core.prepare_upload(upload_scene, upload_plan).ok());
  CHECK(upload_scene.nodes.size() == 1);
  REQUIRE(core.complete_upload(-0.5F, 0.12F).ok());
  CHECK(core.phase() == application_phase::ready);
  core.reset();
  CHECK(core.phase() == application_phase::platform_ready);
}

TEST_CASE("模型查看器 Core 拒绝无效环境建议", "[example][model-viewer][core]") {
  application_core core;
  REQUIRE(core.begin_renderer().ok());
  REQUIRE(core.renderer_ready().ok());
  REQUIRE(core.accept_scene({}).ok());
  CHECK(core.complete_upload(std::numeric_limits<float>::infinity(), 1.0F) ==
        granit::result::invalid_argument);
  CHECK(core.phase() == application_phase::gpu_upload);
}

TEST_CASE("模型查看器 Core 生成不含 GPU 资源的单帧数据", "[example][model-viewer][core]") {
  granit::example::gltf::scene scene;
  auto& primitive = scene.meshes.emplace_back().primitives.emplace_back();
  primitive.positions = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}};
  primitive.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  primitive.indices = {0, 1, 2};
  primitive.material = 0;
  primitive.local_bounds = {.minimum = {-1, -1, 0}, .maximum = {1, 1, 0}, .valid = true};
  scene.materials.emplace_back();
  scene.nodes.emplace_back().mesh = 0;
  application_core core;
  REQUIRE(core.begin_renderer().ok());
  REQUIRE(core.renderer_ready().ok());
  REQUIRE(core.accept_scene(std::move(scene)).ok());
  REQUIRE(core.complete_upload(-0.5F, 0.12F).ok());
  viewer_frame output;
  viewer_document_update zero_sized;
  zero_sized.height = 480;
  CHECK(core.tick(zero_sized, output) == granit::result::not_ready);
  viewer_document_update input;
  input.width = 640;
  input.height = 480;
  input.performance = performance_sample{.frames_per_second = 60.0F, .cpu_frame_ms = 2.0F};
  REQUIRE(core.tick(input, output).ok());
  CHECK(output.width == 640);
  CHECK(output.height == 480);
  CHECK(output.clear_color.red == Catch::Approx(0.025F));
  CHECK(output.clear_color.green == Catch::Approx(0.04F));
  CHECK(output.clear_color.blue == Catch::Approx(0.065F));
  CHECK(output.environment_intensity == Catch::Approx(0.12F));
  CHECK(output.environment_rotation_radians == Catch::Approx(0.0F));
  CHECK(core.performance().size() == 1);
}
