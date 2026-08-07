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

TEST_CASE("结果码保持定宽 C ABI", "[result]") {
  STATIC_CHECK(sizeof(granit::result) == sizeof(std::int32_t));
  CHECK(granit::succeeded(granit::result::success));
  CHECK_FALSE(granit::failed(granit::result::success));
  CHECK(granit::failed(granit::result::invalid_argument));
  CHECK(granit::result_message(granit::result::invalid_handle) == "invalid handle");
  CHECK(
    granit::result_message(granit::result::backend_unavailable) ==
    "rendering backend unavailable");
  CHECK(
    granit::result_message(granit::result::incompatible_driver) ==
    "incompatible graphics driver");
}

TEST_CASE("基础句柄使用 64 位整数和统一空值", "[handle]") {
  STATIC_CHECK(sizeof(granit::handle) == sizeof(std::uint64_t));
  STATIC_CHECK(granit::null_handle == 0);
}
