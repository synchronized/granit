// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "resource_bundle.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace web = granit::example::model_viewer::web;

TEST_CASE("浏览器资源 Bundle 规范化并解析外部资源", "[example][model-viewer][web]") {
  web::resource_bundle bundle;
  constexpr std::array first{std::byte{1}, std::byte{2}};
  constexpr std::array replacement{std::byte{3}};
  CHECK(bundle.insert("textures/./base.png", first));
  CHECK(bundle.contains("textures/base.png"));

  std::vector<std::byte> output;
  CHECK(bundle.resolve("textures/base.png", output));
  CHECK(output == std::vector<std::byte>(first.begin(), first.end()));
  CHECK(bundle.insert("textures/base.png", replacement));
  CHECK(bundle.resolve("textures/./base.png", output));
  CHECK(output == std::vector<std::byte>(replacement.begin(), replacement.end()));
  CHECK(bundle.size() == 1);
}

TEST_CASE("浏览器资源 Bundle 拒绝越界 URI 并保持失败输出", "[example][model-viewer][web]") {
  web::resource_bundle bundle;
  constexpr std::array bytes{std::byte{9}};
  CHECK_FALSE(bundle.insert("../secret.bin", bytes));
  CHECK_FALSE(bundle.insert("https://example.com/a.bin", bytes));
  CHECK_FALSE(bundle.insert("empty.bin", {}));

  std::vector<std::byte> output{std::byte{7}};
  CHECK_FALSE(bundle.resolve("missing.bin", output));
  CHECK(output == std::vector<std::byte>{std::byte{7}});
  bundle.clear();
  CHECK(bundle.size() == 0);
}
