// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/application_core.h"

#include <catch2/catch_all.hpp>

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
