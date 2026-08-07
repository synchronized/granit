// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("C++ 包装返回编译期版本", "[version]") {
  const auto version = granit::library_version();

  CHECK(version.major == GRANIT_VERSION_MAJOR);
  CHECK(version.minor == GRANIT_VERSION_MINOR);
  CHECK(version.patch == GRANIT_VERSION_PATCH);
}
