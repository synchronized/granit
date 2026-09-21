// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <array>
#include <cstddef>
#include <thread>

#include <catch2/catch_all.hpp>
#include <granit/granit.hpp>

namespace {
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

TEST_CASE("Readback Batch 异步返回 Buffer 内容", "[readback_batch][buffer]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-readback-batch"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::buffer buffer;
  REQUIRE(buffer.initialize(renderer, {.size = 64,
                                       .usage = granit::buffer_usage::transfer_source |
                                                granit::buffer_usage::transfer_destination,
                                       .location = granit::memory_location::device}) ==
          granit::result::success);
  std::array<std::byte, 16> expected{};
  for (std::size_t index = 0; index < expected.size(); ++index)
    expected[index] = static_cast<std::byte>(index + 1);
  granit::upload_batch upload;
  REQUIRE(upload.initialize(renderer) == granit::result::success);
  REQUIRE(upload.write_buffer(buffer.ref(), 8, expected) == granit::result::success);
  REQUIRE(upload.submit() == granit::result::success);

  granit::readback_batch batch;
  REQUIRE(batch.create(renderer, {.max_result_bytes = 32, .max_operation_count = 1}) ==
          granit::result::success);
  std::uint32_t result_index{};
  REQUIRE(batch.read_buffer(buffer.ref(), 8, expected.size(), result_index) ==
          granit::result::success);
  CHECK(result_index == 0);
  CHECK(batch.read_buffer(buffer.ref(), 0, 4, result_index) == granit::result::not_ready);

  granit::async_operation operation;
  REQUIRE(batch.submit_async(operation) == granit::result::success);
  granit::async_operation_status status;
  for (std::uint32_t attempt = 0; attempt < 10000; ++attempt) {
    REQUIRE(operation.get_status(status) == granit::result::success);
    if (status.complete())
      break;
    REQUIRE(renderer.process_events() == granit::result::success);
    std::this_thread::yield();
  }
  REQUIRE(status.state == granit::async_operation_state::succeeded);
  granit::readback_result_info info;
  REQUIRE(granit::get_readback_result_info(operation, 0, info) == granit::result::success);
  CHECK(info.type == granit::readback_result_type::buffer);
  CHECK(info.required_size == expected.size());
  std::array<std::byte, 16> actual{};
  std::uint64_t size = actual.size();
  REQUIRE(granit::copy_readback_result(operation, 0, actual, size) == granit::result::success);
  CHECK(size == actual.size());
  CHECK(actual == expected);
}

TEST_CASE("Readback Batch 拒绝空提交与无效句柄", "[readback_batch][contract]") {
  granit::readback_batch batch;
  CHECK(batch.create(GRANIT_NULL_HANDLE) == granit::result::invalid_argument);
}

TEST_CASE("Readback Batch 异步返回紧密 Texture 内容", "[readback_batch][texture]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-texture-readback"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  granit::texture texture;
  REQUIRE(texture.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                        .usage = granit::texture_usage::transfer_source |
                                                 granit::texture_usage::transfer_destination,
                                        .width = 3,
                                        .height = 2}) == granit::result::success);
  std::array<std::byte, 24> expected{};
  for (std::size_t index = 0; index < expected.size(); ++index)
    expected[index] = static_cast<std::byte>(index * 3 + 1);
  REQUIRE(texture.write(expected, {.bytes_per_row = 12, .rows_per_image = 2},
                        {.width = 3, .height = 2}) == granit::result::success);
  granit::readback_batch batch;
  REQUIRE(batch.create(renderer) == granit::result::success);
  std::uint32_t index{};
  const granit::texture_write_region region{.mip_level = 0,
                                            .base_array_layer = 0,
                                            .array_layer_count = 1,
                                            .aspect = granit::texture_aspect::color,
                                            .x = 0,
                                            .y = 0,
                                            .z = 0,
                                            .width = 3,
                                            .height = 2,
                                            .depth = 1};
  REQUIRE(batch.read_texture(texture.ref(), region, index) == granit::result::success);
  REQUIRE(texture.reset() == granit::result::success);
  granit::async_operation operation;
  REQUIRE(batch.submit_async(operation) == granit::result::success);
  granit::async_operation_status status;
  for (std::uint32_t attempt = 0; attempt < 10000; ++attempt) {
    REQUIRE(operation.get_status(status) == granit::result::success);
    if (status.complete())
      break;
    std::this_thread::yield();
  }
  REQUIRE(status.state == granit::async_operation_state::succeeded);
  granit::readback_result_info info;
  REQUIRE(granit::get_readback_result_info(operation, index, info) == granit::result::success);
  CHECK(info.required_size == expected.size());
  CHECK(info.bytes_per_row == 12);
  std::array<std::byte, 24> actual{};
  std::uint64_t size = actual.size();
  REQUIRE(granit::copy_readback_result(operation, index, actual, size) == granit::result::success);
  CHECK(actual == expected);
}
} // namespace
