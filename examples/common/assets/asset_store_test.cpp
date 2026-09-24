// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_store.h"

#include <catch2/catch_all.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

TEST_CASE("Example Asset Store resolves paths beside the executable") {
  const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto root = std::filesystem::temp_directory_path() / ("granit-assets-" + unique);
  const auto asset_dir = root / "bin" / "assets" / "tutorials";
  REQUIRE(std::filesystem::create_directories(asset_dir));
  {
    std::ofstream output{asset_dir / "fixture.bin", std::ios::binary};
    REQUIRE(output.good());
    output.write("GRANIT", 6);
  }

  granit::example::assets::asset_store assets;
  REQUIRE(assets.initialize((root / "bin" / "example").string()));
  std::vector<std::byte> bytes;
  CHECK(assets.read("tutorials/fixture.bin", bytes));
  CHECK(bytes.size() == 6);
  CHECK(assets.read("tutorials/./fixture.bin", bytes));
  CHECK(bytes.size() == 6);
  CHECK_FALSE(assets.read("../fixture.bin", bytes));
  CHECK_FALSE(assets.read("tutorials/../fixture.bin", bytes));
  CHECK_FALSE(assets.read("tutorials\\fixture.bin", bytes));
  CHECK_FALSE(assets.read("C:/fixture.bin", bytes));
  CHECK_FALSE(assets.read("/tutorials/fixture.bin", bytes));
  CHECK_FALSE(assets.read("tutorials/fixture.bin?version=1", bytes));
  CHECK(bytes.size() == 6);

  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK_FALSE(error);
}
