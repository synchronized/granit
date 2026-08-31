// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <utility>

#include <catch2/catch_all.hpp>

#include "backend/plugin_api.h"
#include "platform/shared_library.h"

TEST_CASE("平台动态库只加载显式绝对路径", "[platform][shared_library]") {
  CHECK_FALSE(granit::detail::platform::module_directory().empty());
  granit::detail::platform::shared_library library;
  CHECK_FALSE(library.open(nullptr));
  CHECK_FALSE(library.open(""));
  CHECK_FALSE(library.open("a"));
  CHECK_FALSE(library.open("C:"));
  CHECK_FALSE(library.open("\\"));
  CHECK_FALSE(library.open("relative-plugin-path"));
  CHECK_FALSE(library.open(GRANIT_MISSING_BACKEND_PLUGIN_PATH));
  CHECK_FALSE(library.is_open());
}

TEST_CASE("平台动态库支持符号查询和移动所有权", "[platform][shared_library]") {
  granit::detail::platform::shared_library library;
  REQUIRE(library.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH));
  CHECK(library.symbol(GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL) != nullptr);
  CHECK(library.symbol("granit_symbol_that_does_not_exist") == nullptr);

  auto moved = std::move(library);
  CHECK_FALSE(library.is_open());
  CHECK(moved.is_open());

  granit::detail::platform::shared_library assigned;
  assigned = std::move(moved);
  CHECK_FALSE(moved.is_open());
  CHECK(assigned.is_open());
}
