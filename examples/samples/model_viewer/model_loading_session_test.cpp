// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/model_loading_session.h"

#include "gltf/asset_loaders.h"
#include "platform/register_asset_sources.h"
#include "tasks/task_system.h"

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <fstream>

namespace assets = granit::example::assets;
namespace tasks = granit::example::tasks;
namespace viewer = granit::example::model_viewer;

namespace {

void initialize_assets(tasks::task_system& task_system, assets::asset_manager& manager) {
  REQUIRE(task_system.initialize({.worker_count = 0}).ok());
  REQUIRE(granit::example::platform::register_asset_sources(manager, "test.exe").ok());
  REQUIRE(granit::example::gltf::register_standard_asset_loaders(manager).ok());
}

void finish_loading(tasks::task_system& task_system, viewer::model_loading_session& session) {
  while (session.status() == viewer::model_loading_status::loading_assets) {
    static_cast<void>(task_system.pump_main());
    session.poll();
  }
}

} // namespace

TEST_CASE("模型加载会话通过 Asset Manager 准备 GPU 计划", "[example][model-viewer][loading]") {
  const auto directory =
      std::filesystem::temp_directory_path() / "granit_model_loading_session_test";
  std::error_code filesystem_error;
  std::filesystem::create_directories(directory, filesystem_error);
  REQUIRE_FALSE(filesystem_error);
  const auto model_path = directory / "scene.gltf";
  {
    std::ofstream document{model_path, std::ios::binary};
    document << R"({"asset":{"version":"2.0"},"scenes":[{"nodes":[]}],"scene":0})";
  }

  tasks::task_system task_system;
  assets::asset_manager manager{task_system};
  initialize_assets(task_system, manager);
  viewer::model_loading_session session;
  REQUIRE(session.start(manager, assets::asset_location::external(model_path.string())));
  finish_loading(task_system, session);
  REQUIRE(session.status() == viewer::model_loading_status::assets_ready);
  REQUIRE(session.prepare().ok());
  REQUIRE(session.status() == viewer::model_loading_status::ready);

  granit::example::gltf::scene scene;
  granit::example::gltf_rendering::scene_plan plan;
  CHECK(session.take(scene, plan));
  CHECK_FALSE(session.take(scene, plan));
  std::filesystem::remove_all(directory, filesystem_error);
}

TEST_CASE("模型加载会话可在资产读取阶段取消", "[example][model-viewer][loading]") {
  tasks::task_system task_system;
  assets::asset_manager manager{task_system};
  initialize_assets(task_system, manager);
  viewer::model_loading_session session;
  REQUIRE(session.start(manager, assets::asset_location::external("missing.gltf")));
  session.cancel();
  CHECK(session.status() == viewer::model_loading_status::cancelled);
  CHECK(session.result() == granit::result::cancelled);
}

TEST_CASE("模型加载会话保留外部资源读取错误", "[example][model-viewer][loading]") {
  const auto directory =
      std::filesystem::temp_directory_path() / "granit_model_loading_resource_error_test";
  std::error_code filesystem_error;
  std::filesystem::create_directories(directory, filesystem_error);
  REQUIRE_FALSE(filesystem_error);
  const auto model_path = directory / "scene.gltf";
  {
    std::ofstream document{model_path, std::ios::binary};
    document << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"missing.bin","byteLength":4}]})";
  }

  tasks::task_system task_system;
  assets::asset_manager manager{task_system};
  initialize_assets(task_system, manager);
  viewer::model_loading_session session;
  REQUIRE(session.start(manager, assets::asset_location::external(model_path.string())));
  finish_loading(task_system, session);

  CHECK(session.status() == viewer::model_loading_status::failed);
  CHECK(session.error() == viewer::model_loading_error::resource_read);
  CHECK(session.result() == granit::result::invalid_argument);
  std::filesystem::remove_all(directory, filesystem_error);
}
