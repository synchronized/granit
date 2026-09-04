// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/application_core.h"

#include <catch2/catch_all.hpp>
#include <granit/renderer/renderer.hpp>

#include <array>

TEST_CASE("模型查看器 Core 严格执行启动状态机", "[example][model-viewer][core]") {
  using namespace granit::example::model_viewer;
  application_core core;
  CHECK(core.phase() == application_phase::platform_ready);
  CHECK(core.renderer_ready() == granit::result::invalid_argument);
  REQUIRE(core.begin_renderer() == granit::result::success);
  REQUIRE(core.renderer_ready() == granit::result::success);
  granit::example::gltf::scene scene;
  scene.nodes.resize(1);
  REQUIRE(core.accept_scene(std::move(scene)) == granit::result::success);
  CHECK(core.phase() == application_phase::gpu_upload);
  CHECK(core.upload(GRANIT_NULL_HANDLE) == granit::result::invalid_handle);
  CHECK(core.phase() == application_phase::failed);
  CHECK_FALSE(core.diagnostic().empty());
  core.reset();
  CHECK(core.phase() == application_phase::platform_ready);
}

TEST_CASE("模型查看器 Core 保留资产解析诊断", "[example][model-viewer][core]") {
  using namespace granit::example::model_viewer;
  application_core core;
  REQUIRE(core.begin_renderer() == granit::result::success);
  REQUIRE(core.renderer_ready() == granit::result::success);
  const std::array invalid{std::byte{0}, std::byte{1}};
  CHECK(core.load_asset(invalid, nullptr) == granit::result::invalid_argument);
  CHECK(core.phase() == application_phase::failed);
  CHECK_FALSE(core.diagnostic().empty());
}

TEST_CASE("模型查看器 Core 拒绝无效环境包", "[example][model-viewer][core][gpu]") {
  using namespace granit::example::model_viewer;
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "Environment Test"});
  if (renderer_result.failed())
    SKIP("当前环境没有可用 Renderer");

  granit::example::gltf::scene scene;
  scene.nodes.emplace_back();
  application_core core;
  REQUIRE(core.begin_renderer() == granit::result::success);
  REQUIRE(core.renderer_ready() == granit::result::success);
  REQUIRE(core.accept_scene(std::move(scene)) == granit::result::success);
  const std::array invalid{std::byte{1}};
  CHECK(core.upload(renderer.native_handle(), invalid) == granit::result::invalid_argument);
  CHECK(core.phase() == application_phase::failed);
  CHECK_FALSE(core.diagnostic().empty());
}

TEST_CASE("模型查看器 Core 生成后端无关单帧描述", "[example][model-viewer][core][gpu]") {
  using namespace granit::example::model_viewer;
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "Model Viewer Core Test"});
  if (renderer_result.failed())
    SKIP("当前环境没有可用 Renderer");

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
  REQUIRE(core.begin_renderer() == granit::result::success);
  REQUIRE(core.renderer_ready() == granit::result::success);
  REQUIRE(core.accept_scene(std::move(scene)) == granit::result::success);
  REQUIRE(core.upload(renderer.native_handle()) == granit::result::success);
  const auto original_mesh = core.scene_gpu().meshes().front().native_handle();
  REQUIRE(core.reupload_scene(renderer.native_handle(), 1.0F) == granit::result::success);
  CHECK(core.scene_gpu().meshes().front().native_handle() != original_mesh);
  const auto rebuilt_mesh = core.scene_gpu().meshes().front().native_handle();
  CHECK(core.reupload_scene(renderer.native_handle(), 0.0F) == granit::result::invalid_argument);
  CHECK(core.scene_gpu().meshes().front().native_handle() == rebuilt_mesh);

  application_tick_output output;
  application_tick_input zero_sized;
  zero_sized.height = 480;
  CHECK(core.tick(zero_sized, output) == granit::result::not_ready);
  const performance_sample sample{.frames_per_second = 60.0F, .cpu_frame_ms = 2.0F};
  application_tick_input input;
  input.width = 640;
  input.height = 480;
  input.performance = sample;
  REQUIRE(core.tick(input, output) == granit::result::success);
  CHECK(output.snapshot.valid());
  CHECK(output.render.scene == output.snapshot.native_handle());
  CHECK(output.render.width == 640);
  CHECK(output.render.height == 480);
  CHECK(output.render.clear_color.red == Catch::Approx(0.025F));
  CHECK(output.render.clear_color.green == Catch::Approx(0.04F));
  CHECK(output.render.clear_color.blue == Catch::Approx(0.065F));
  CHECK(output.render.draw_binding_count == 1);
  CHECK(output.render.draw_bindings == core.scene_gpu().draw_bindings().data());
  REQUIRE(output.render.environment != nullptr);
  CHECK(output.render.environment->irradiance != GRANIT_NULL_HANDLE);
  CHECK(output.render.environment->prefiltered_environment != GRANIT_NULL_HANDLE);
  CHECK(output.render.environment->brdf_lut != GRANIT_NULL_HANDLE);
  CHECK(output.render.environment->intensity == Catch::Approx(0.2F));
  CHECK(output.render.environment->rotation_radians == Catch::Approx(0.0F));
  CHECK(core.performance().size() == 1);
}
