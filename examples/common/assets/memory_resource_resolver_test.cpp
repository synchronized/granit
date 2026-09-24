// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/memory_resource_resolver.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace assets = granit::example::assets;

TEST_CASE("内存资源 Resolver 规范化并解析外部资源", "[example][assets]") {
  assets::memory_resource_resolver resolver;
  constexpr std::array first{std::byte{1}, std::byte{2}};
  constexpr std::array replacement{std::byte{3}};
  CHECK(resolver.insert("textures/./base.png", first));
  CHECK(resolver.contains("textures/base.png"));

  std::vector<std::byte> output;
  CHECK(resolver.resolve("textures/base.png", output));
  CHECK(output == std::vector<std::byte>(first.begin(), first.end()));
  CHECK(resolver.insert("textures/base.png", replacement));
  CHECK(resolver.resolve("textures/./base.png", output));
  CHECK(output == std::vector<std::byte>(replacement.begin(), replacement.end()));
  CHECK(resolver.size() == 1);
}

TEST_CASE("内存资源 Resolver 拒绝越界 URI", "[example][assets]") {
  assets::memory_resource_resolver resolver;
  constexpr std::array bytes{std::byte{9}};
  CHECK_FALSE(resolver.insert("../secret.bin", bytes));
  CHECK_FALSE(resolver.insert("https://example.com/a.bin", bytes));
  CHECK_FALSE(resolver.insert("empty.bin", {}));

  std::vector<std::byte> output{std::byte{7}};
  CHECK_FALSE(resolver.resolve("missing.bin", output));
  CHECK(output == std::vector<std::byte>{std::byte{7}});
}
