// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <array>
#include <cstddef>

#include <catch2/catch_all.hpp>
#include <granit/granit.hpp>

namespace {

bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

TEST_CASE("Upload Batch 合并 Buffer 写入并支持复用", "[upload_batch][buffer]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-upload-batch"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::buffer buffer;
  REQUIRE(buffer.initialize(renderer.native_handle(),
                            {.size = 256,
                             .usage = granit::buffer_usage::transfer_destination,
                             .location = granit::memory_location::device}) ==
          granit::result::success);
  granit::upload_batch batch;
  REQUIRE(batch.initialize(renderer.native_handle()) == granit::result::success);
  CHECK(batch.submit() == granit::result::invalid_argument);

  std::array<std::byte, 16> first{};
  std::array<std::byte, 32> second{};
  REQUIRE(batch.write_buffer(buffer.native_handle(), 0, first) == granit::result::success);
  REQUIRE(batch.write_buffer(buffer.native_handle(), 64, second) == granit::result::success);
  REQUIRE(batch.submit() == granit::result::success);

  REQUIRE(batch.write_buffer(buffer.native_handle(), 128, first) == granit::result::success);
  REQUIRE(batch.reset() == granit::result::success);
  CHECK(batch.submit() == granit::result::invalid_argument);
}

TEST_CASE("Upload Batch 在公开 Buffer 句柄销毁后仍保活资源", "[upload_batch][lifetime]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-upload-retain"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::buffer buffer;
  REQUIRE(buffer.initialize(renderer.native_handle(),
                            {.size = 64,
                             .usage = granit::buffer_usage::transfer_destination,
                             .location = granit::memory_location::device}) ==
          granit::result::success);
  granit::upload_batch batch;
  REQUIRE(batch.initialize(renderer.native_handle()) == granit::result::success);
  std::array<std::byte, 16> data{};
  REQUIRE(batch.write_buffer(buffer.native_handle(), 0, data) == granit::result::success);
  REQUIRE(buffer.reset() == granit::result::success);
  CHECK(batch.submit() == granit::result::success);
}

} // namespace
