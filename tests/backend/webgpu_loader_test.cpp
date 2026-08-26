// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include "backend/webgpu/loader.h"

TEST_CASE("WebGPU Loader 区分缺失库和缺失入口", "[backend][webgpu]") {
  granit::detail::webgpu_loader loader;

  CHECK(loader.open(nullptr) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.open("granit-provider-that-does-not-exist") == GRANIT_ERROR_BACKEND_UNAVAILABLE);
  CHECK_FALSE(loader.is_open());

  CHECK(loader.open(GRANIT_WEBGPU_INCOMPLETE_PROVIDER_PATH) == GRANIT_ERROR_INCOMPATIBLE_DRIVER);
  CHECK_FALSE(loader.is_open());
}

TEST_CASE("WebGPU Loader 解析锁定 Provider 的基础入口", "[backend][webgpu]") {
  granit::detail::webgpu_loader loader;
  REQUIRE(loader.open(GRANIT_WEBGPU_FAKE_PROVIDER_PATH) == GRANIT_SUCCESS);
  CHECK(loader.is_open());
  CHECK(loader.create_instance() != nullptr);

  loader.close();
  CHECK_FALSE(loader.is_open());
  CHECK(loader.create_instance() == nullptr);
}
