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
  STATIC_CHECK(granit::result::success.ok());
  STATIC_CHECK(granit::result::invalid_argument.failed());
  STATIC_CHECK(static_cast<bool>(granit::result::success));
  STATIC_CHECK_FALSE(static_cast<bool>(granit::result::invalid_argument));
  STATIC_CHECK(granit::result{}.failed());
  STATIC_CHECK(granit::result::not_ready.native() == GRANIT_ERROR_NOT_READY);
  STATIC_CHECK(granit::to_native(granit::result::success) == GRANIT_SUCCESS);
  STATIC_CHECK(granit::from_native(GRANIT_ERROR_INVALID_HANDLE) ==
               granit::result::invalid_handle);
  STATIC_CHECK(granit::from_native(INT32_C(-999)).native() == INT32_C(-999));
  CHECK(granit::succeeded(granit::result::success));
  CHECK_FALSE(granit::failed(granit::result::success));
  CHECK(granit::failed(granit::result::invalid_argument));
  CHECK(granit::result_message(granit::result::invalid_handle) == "invalid handle");
  CHECK(granit::result::invalid_handle.message() == "invalid handle");
  CHECK(
    granit::result_message(granit::result::backend_unavailable) ==
    "rendering backend unavailable");
  CHECK(
    granit::result_message(granit::result::incompatible_driver) ==
    "incompatible graphics driver");
  CHECK(
    granit::result_message(granit::result::no_suitable_device) ==
    "no suitable graphics device");
}

TEST_CASE("基础句柄使用 64 位整数和统一空值", "[handle]") {
  STATIC_CHECK(sizeof(granit::handle) == sizeof(std::uint64_t));
  STATIC_CHECK(granit::null_handle == 0);
}
