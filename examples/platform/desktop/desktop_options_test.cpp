// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "desktop_options.h"

#include <catch2/catch_all.hpp>

#include <array>

TEST_CASE("模型查看器桌面参数显式选择 Renderer 后端", "[example][model-viewer][desktop]") {
  using namespace granit::example::model_viewer::desktop;
  const std::array arguments{
      std::string_view{"--backend=webgpu"},  std::string_view{"--backend-library"},
      std::string_view{"dawn.dll"},          std::string_view{"--asset"},
      std::string_view{"FlightHelmet.gltf"}, std::string_view{"--validation"},
      std::string_view{"--smoke-test"}};
  options parsed;
  REQUIRE(parse_options(arguments, parsed) == granit::result::success);
  CHECK(parsed.backend == granit::renderer_backend::webgpu);
  CHECK(parsed.backend_library_path == "dawn.dll");
  CHECK(parsed.asset_path == "FlightHelmet.gltf");
  CHECK(parsed.enable_validation);
  CHECK(parsed.smoke_test);
}

TEST_CASE("模型查看器桌面参数拒绝未知后端且不修改输出", "[example][model-viewer][desktop]") {
  using namespace granit::example::model_viewer::desktop;
  options parsed;
  parsed.asset_path = "保留.glb";
  const std::array invalid_backend{std::string_view{"--backend=metal"}, std::string_view{"--asset"},
                                   std::string_view{"model.glb"}};
  CHECK(parse_options(invalid_backend, parsed) == granit::result::invalid_argument);
  CHECK(parsed.asset_path == "保留.glb");

  const std::array missing_asset{std::string_view{"--backend=vulkan"}};
  CHECK(parse_options(missing_asset, parsed) == granit::result::invalid_argument);
  CHECK(parsed.asset_path == "保留.glb");
}

TEST_CASE("模型查看器桌面参数支持自动与 Vulkan 后端", "[example][model-viewer][desktop]") {
  using namespace granit::example::model_viewer::desktop;
  options parsed;
  const std::array automatic{std::string_view{"--backend=auto"}, std::string_view{"--asset"},
                             std::string_view{"model.glb"}};
  REQUIRE(parse_options(automatic, parsed) == granit::result::success);
  CHECK(parsed.backend == granit::renderer_backend::automatic);

  const std::array vulkan{std::string_view{"--backend=vulkan"}, std::string_view{"--asset"},
                          std::string_view{"model.glb"}};
  REQUIRE(parse_options(vulkan, parsed) == granit::result::success);
  CHECK(parsed.backend == granit::renderer_backend::vulkan);
}
