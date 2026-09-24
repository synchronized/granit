// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_batch.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace assets = granit::example::assets;

TEST_CASE("资产批次拒绝重复和越界资源路径", "[example][assets]") {
  assets::asset_system system;
  REQUIRE(system.initialize("example"));
  assets::asset_batch batch;
  CHECK(batch.add("./mesh.bin", system.bundled(), "assets/mesh.bin"));
  CHECK_FALSE(batch.add("mesh.bin", system.bundled(), "assets/duplicate.bin"));
  CHECK_FALSE(batch.add("../escape.bin", system.bundled(), "assets/escape.bin"));
  CHECK_FALSE(batch.add("other.bin", system.bundled(), {}));
  CHECK_FALSE(batch.add("other.bin", {}, "assets/other.bin"));
  CHECK(batch.status() == assets::asset_batch_status::idle);
}

TEST_CASE("空资产批次启动后可以提交", "[example][assets]") {
  assets::asset_system system;
  REQUIRE(system.initialize("example"));
  assets::asset_batch batch;
  assets::memory_resource_resolver resolver;
  CHECK(batch.start(system));
  CHECK(batch.status() == assets::asset_batch_status::ready);
  CHECK(batch.commit(resolver));
  CHECK(resolver.size() == 0);
}

#if !defined(__EMSCRIPTEN__)
TEST_CASE("Desktop 资产批次加载并提交资源", "[example][assets]") {
  const auto path = std::filesystem::temp_directory_path() / "granit_asset_batch_test.bin";
  {
    std::ofstream stream(path, std::ios::binary);
    const std::array bytes{char{4}, char{5}};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }

  assets::asset_system system;
  REQUIRE(system.initialize("example"));
  assets::asset_mount content;
  REQUIRE(system.mount(path.parent_path().string(), content));
  assets::asset_batch batch;
  REQUIRE(batch.add("mesh.bin", content, path.filename().string()));
  REQUIRE(batch.start(system));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
  while (batch.status() == assets::asset_batch_status::pending &&
         std::chrono::steady_clock::now() < deadline) {
    system.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }

  REQUIRE(batch.status() == assets::asset_batch_status::ready);
  CHECK(batch.progress().completed_items == 1);
  assets::memory_resource_resolver resolver;
  REQUIRE(batch.commit(resolver));
  std::vector<std::byte> output;
  CHECK(resolver.resolve("mesh.bin", output));
  CHECK(output.size() == 2);
  std::error_code error;
  std::filesystem::remove(path, error);
}
#endif
