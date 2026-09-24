// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/model_loading_session.h"

#include <catch2/catch_all.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace assets = granit::example::assets;
namespace viewer = granit::example::model_viewer;

TEST_CASE("模型加载会话统一读取、Import 与 GPU 计划", "[example][model-viewer][loading]") {
  const auto directory =
      std::filesystem::temp_directory_path() / "granit_model_loading_session_test";
  std::error_code filesystem_error;
  std::filesystem::create_directories(directory, filesystem_error);
  REQUIRE_FALSE(filesystem_error);
  {
    std::ofstream document{directory / "scene.gltf", std::ios::binary};
    document << R"({"asset":{"version":"2.0"},"scenes":[{"nodes":[]}],"scene":0})";
  }

  assets::asset_system asset_system;
  REQUIRE(asset_system.initialize("example"));
  assets::asset_mount content;
  REQUIRE(asset_system.mount(directory.string(), content));
  viewer::model_loading_session session;
  REQUIRE(session.start(asset_system, {content, "scene.gltf"}));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
  while (session.status() == viewer::model_loading_status::loading_assets &&
         std::chrono::steady_clock::now() < deadline) {
    session.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }
  REQUIRE(session.status() == viewer::model_loading_status::assets_ready);
  REQUIRE(session.prepare().ok());
  REQUIRE(session.status() == viewer::model_loading_status::ready);

  granit::example::gltf::scene scene;
  viewer::gpu_scene_plan plan;
  CHECK(session.take(scene, plan));
  CHECK_FALSE(session.take(scene, plan));

  std::filesystem::remove_all(directory, filesystem_error);
}

TEST_CASE("模型加载会话可在资产读取阶段取消", "[example][model-viewer][loading]") {
  assets::asset_system asset_system;
  REQUIRE(asset_system.initialize("example"));
  viewer::model_loading_session session;
  REQUIRE(session.start(asset_system, {asset_system.bundled(), "missing.gltf"}));
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
  {
    std::ofstream document{directory / "scene.gltf", std::ios::binary};
    document << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"missing.bin","byteLength":4}]})";
  }

  assets::asset_system asset_system;
  REQUIRE(asset_system.initialize("example"));
  assets::asset_mount content;
  REQUIRE(asset_system.mount(directory.string(), content));
  viewer::model_loading_session session;
  REQUIRE(session.start(asset_system, {content, "scene.gltf"}));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
  while (session.status() == viewer::model_loading_status::loading_assets &&
         std::chrono::steady_clock::now() < deadline) {
    asset_system.poll();
    session.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }

  CHECK(session.status() == viewer::model_loading_status::failed);
  CHECK(session.error() == viewer::model_loading_error::resource_read);
  CHECK(session.result() == granit::result::invalid_argument);
  std::filesystem::remove_all(directory, filesystem_error);
}
