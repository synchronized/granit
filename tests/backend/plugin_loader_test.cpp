// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <string_view>

#include <catch2/catch_all.hpp>

#include "backend/plugin_loader.h"

TEST_CASE("后端插件 Loader 区分缺失库和不兼容 ABI", "[backend][plugin]") {
  granit::detail::backend_plugin_loader loader;

  CHECK(loader.open(nullptr, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.open("granit-plugin-that-does-not-exist", GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
        GRANIT_ERROR_BACKEND_UNAVAILABLE);
  CHECK_FALSE(loader.is_open());

  CHECK(loader.open(GRANIT_INCOMPATIBLE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
        GRANIT_ERROR_INCOMPATIBLE_DRIVER);
  CHECK_FALSE(loader.is_open());
}

TEST_CASE("后端插件 Loader 完成版本化握手", "[backend][plugin]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  REQUIRE(loader.api() != nullptr);
  CHECK(loader.api()->abi_version == GRANIT_BACKEND_PLUGIN_ABI_VERSION);
  CHECK(loader.api()->kind == GRANIT_BACKEND_PLUGIN_KIND_WEBGPU);
  CHECK(std::string_view{loader.api()->name, loader.api()->name_length} == "测试 WebGPU 插件");

  loader.close();
  CHECK_FALSE(loader.is_open());
  CHECK(loader.api() == nullptr);
}
