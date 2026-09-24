// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/resource_path.h"

#include <catch2/catch_all.hpp>

TEST_CASE("资源路径只接受受控相对路径", "[example][assets][path]") {
  std::string normalized = "unchanged";
  REQUIRE(
      granit::example::assets::normalize_resource_path("textures/./base_color.png", normalized));
  CHECK(normalized == "textures/base_color.png");

  for (const std::string_view invalid :
       {"", "../secret.bin", "textures/../../secret.bin", "/absolute.bin", "C:/absolute.bin",
        "https://host/a.bin", "data:application/octet-stream;base64,AA==", "a\\b.bin",
        "a%2f..%2fsecret.bin", "a.bin?x=1", "a.bin#fragment"}) {
    normalized = "unchanged";
    CHECK_FALSE(granit::example::assets::normalize_resource_path(invalid, normalized));
    CHECK(normalized == "unchanged");
  }
}
