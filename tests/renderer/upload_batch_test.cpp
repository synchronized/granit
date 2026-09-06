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

TEST_CASE("Upload Batch包装把空Renderer归类为无效句柄", "[upload_batch][contract]") {
  granit::upload_batch batch;
  CHECK(batch.initialize(GRANIT_NULL_HANDLE) == granit::result::invalid_handle);
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
  std::array<std::byte, 4 * 4 * 4> pixels{};
  granit::texture texture;
  REQUIRE(texture.initialize(renderer.native_handle(),
                             {.format = granit::texture_format::rgba8_unorm,
                              .usage = granit::texture_usage::transfer_destination,
                              .width = 4,
                              .height = 4}) == granit::result::success);
  REQUIRE(batch.write_buffer(buffer.native_handle(), 0, first) == granit::result::success);
  REQUIRE(batch.write_texture(texture.native_handle(), pixels, {}, {.width = 4, .height = 4}) ==
          granit::result::success);
  REQUIRE(batch.write_buffer(buffer.native_handle(), 64, second) == granit::result::success);
  REQUIRE(batch.submit() == granit::result::success);

  REQUIRE(batch.write_buffer(buffer.native_handle(), 128, first) == granit::result::success);
  REQUIRE(batch.reset() == granit::result::success);
  CHECK(batch.submit() == granit::result::invalid_argument);
}

TEST_CASE("Upload Batch 在复制前执行字节数和操作数背压", "[upload_batch][backpressure]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-upload-budget"});
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
  REQUIRE(batch.initialize(renderer.native_handle(),
                           {.max_staged_bytes = 16, .max_operation_count = 2}) ==
          granit::result::success);
  std::array<std::byte, 8> bytes{};
  REQUIRE(batch.write_buffer(buffer.native_handle(), 0, bytes) == granit::result::success);
  REQUIRE(batch.write_buffer(buffer.native_handle(), 8, bytes) == granit::result::success);
  CHECK(batch.write_buffer(buffer.native_handle(), 16, bytes) == granit::result::not_ready);

  granit::upload_batch_info info;
  REQUIRE(batch.get_info(info) == granit::result::success);
  CHECK(info.staged_bytes == 16);
  CHECK(info.operation_count == 2);
  CHECK(info.max_staged_bytes == 16);
  CHECK(info.max_operation_count == 2);

  REQUIRE(batch.submit() == granit::result::success);
  REQUIRE(batch.get_info(info) == granit::result::success);
  CHECK(info.staged_bytes == 0);
  CHECK(info.operation_count == 0);

  std::array<std::byte, 20> oversized{};
  CHECK(batch.write_buffer(buffer.native_handle(), 0, oversized) ==
        granit::result::invalid_argument);
}

TEST_CASE("Upload Batch 异步提交公开非阻塞完成状态", "[upload_batch][async]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-upload-async"});
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
  std::array<std::byte, 16> bytes{};
  REQUIRE(batch.write_buffer(buffer.native_handle(), 0, bytes) == granit::result::success);

  granit::async_operation operation;
  REQUIRE(batch.submit_async(operation) == granit::result::success);
  granit::renderer_resource_stats resource_stats;
  REQUIRE(renderer.get_resource_stats(resource_stats) == granit::result::success);
  CHECK(resource_stats.async_operation_count == 1);
  granit::upload_batch_info info;
  REQUIRE(batch.get_info(info) == granit::result::success);
  CHECK(info.staged_bytes == 0);
  CHECK(info.operation_count == 0);
  REQUIRE(operation.request_cancel() == granit::result::success);

  granit::async_operation_status status;
  for (std::uint32_t attempt = 0; attempt < 10000; ++attempt) {
    REQUIRE(operation.get_status(status) == granit::result::success);
    if (status.complete())
      break;
    REQUIRE(renderer.process_events() == granit::result::success);
    std::this_thread::yield();
  }
  REQUIRE(status.complete());
  CHECK(status.state == granit::async_operation_state::succeeded);
  CHECK(status.operation_result == granit::result::success);
  CHECK(status.cancel_requested);
  REQUIRE(operation.reset() == granit::result::success);
  REQUIRE(renderer.get_resource_stats(resource_stats) == granit::result::success);
  CHECK(resource_stats.async_operation_count == 0);
}

TEST_CASE("销毁运行中上传操作仍保活资源并回收后端槽", "[upload_batch][async][lifetime]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-upload-detached"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  for (std::uint32_t index = 0; index < 6; ++index) {
    granit::buffer buffer;
    REQUIRE(buffer.initialize(renderer.native_handle(),
                              {.size = 64,
                               .usage = granit::buffer_usage::transfer_destination,
                               .location = granit::memory_location::device}) ==
            granit::result::success);
    granit::upload_batch batch;
    REQUIRE(batch.initialize(renderer.native_handle()) == granit::result::success);
    std::array<std::byte, 16> bytes{};
    REQUIRE(batch.write_buffer(buffer.native_handle(), 0, bytes) == granit::result::success);
    granit::async_operation operation;
    auto submit_result = batch.submit_async(operation);
    for (std::uint32_t attempt = 0;
         submit_result == granit::result::not_ready && attempt < 10000; ++attempt) {
      REQUIRE(renderer.process_events() == granit::result::success);
      std::this_thread::yield();
      submit_result = batch.submit_async(operation);
    }
    REQUIRE(submit_result == granit::result::success);
    REQUIRE(operation.reset() == granit::result::success);
    REQUIRE(buffer.reset() == granit::result::success);
    REQUIRE(renderer.process_events() == granit::result::success);
  }
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

TEST_CASE("Upload Batch 在公开 Texture 句柄销毁后仍保活资源", "[upload_batch][texture][lifetime]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-upload-texture"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::texture texture;
  REQUIRE(texture.initialize(renderer.native_handle(),
                             {.format = granit::texture_format::rgba8_unorm,
                              .usage = granit::texture_usage::transfer_destination,
                              .width = 4,
                              .height = 4}) == granit::result::success);
  granit::upload_batch batch;
  REQUIRE(batch.initialize(renderer.native_handle()) == granit::result::success);
  std::array<std::byte, 4 * 4 * 4> pixels{};
  REQUIRE(batch.write_texture(texture.native_handle(), pixels, {}, {.width = 4, .height = 4}) ==
          granit::result::success);
  REQUIRE(texture.reset() == granit::result::success);
  CHECK(batch.submit() == granit::result::success);
}

} // namespace
