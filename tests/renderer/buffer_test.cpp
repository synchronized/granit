// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/renderer.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <catch2/catch_all.hpp>

namespace {

bool environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable ||
         result == granit::result::incompatible_driver ||
         result == granit::result::no_suitable_device;
}

TEST_CASE("UPLOAD Buffer 支持映射写入和显式销毁", "[buffer][c_api]") {
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "granit-buffer-tests"});
  if (environment_unavailable(renderer_result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(renderer_result == granit::result::success);

  granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
  desc.size = 256;
  desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT;
  desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer.native_handle(), &desc, &buffer) == GRANIT_SUCCESS);

  void* mapped = nullptr;
  CHECK(granit_buffer_map(renderer.native_handle(), buffer, 0, 0, &mapped) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_buffer_map(renderer.native_handle(), buffer, 250, 16, &mapped) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(granit_buffer_map(renderer.native_handle(), buffer, 16, 32, &mapped) == GRANIT_SUCCESS);
  REQUIRE(mapped != nullptr);
  static_cast<std::uint8_t*>(mapped)[0] = UINT8_C(42);
  CHECK(granit_buffer_flush(renderer.native_handle(), buffer, 16, 1) == GRANIT_SUCCESS);
  CHECK(granit_buffer_flush(renderer.native_handle(), buffer, 15, 1) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_buffer_flush(renderer.native_handle(), buffer, 16, 0) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_buffer_map(renderer.native_handle(), buffer, 0, 1, &mapped) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_buffer_destroy(renderer.native_handle(), buffer) == GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(granit_buffer_unmap(renderer.native_handle(), buffer) == GRANIT_SUCCESS);
  CHECK(granit_buffer_flush(renderer.native_handle(), buffer, 16, 1) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(granit_buffer_destroy(renderer.native_handle(), buffer) == GRANIT_SUCCESS);
  CHECK(granit_buffer_destroy(renderer.native_handle(), buffer) == GRANIT_ERROR_INVALID_HANDLE);

  desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  desc.memory_location = GRANIT_MEMORY_LOCATION_READBACK;
  REQUIRE(granit_buffer_create(renderer.native_handle(), &desc, &buffer) == GRANIT_SUCCESS);
  REQUIRE(granit_buffer_map(renderer.native_handle(), buffer, 0, desc.size, &mapped) ==
          GRANIT_SUCCESS);
  REQUIRE(mapped != nullptr);
  REQUIRE(granit_buffer_unmap(renderer.native_handle(), buffer) == GRANIT_SUCCESS);
  REQUIRE(granit_buffer_destroy(renderer.native_handle(), buffer) == GRANIT_SUCCESS);
}

TEST_CASE("Buffer 映射验证内存位置、范围和 Renderer 归属", "[buffer][validation]") {
  granit::renderer first;
  const auto first_result = first.initialize({.application_name = "granit-buffer-first"});
  if (environment_unavailable(first_result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(first_result == granit::result::success);
  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-buffer-second"}) ==
          granit::result::success);

  granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
  desc.size = 64;
  desc.usage = GRANIT_BUFFER_USAGE_VERTEX_BIT;
  desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(first.native_handle(), &desc, &buffer) == GRANIT_SUCCESS);

  void* mapped = nullptr;
  CHECK(granit_buffer_map(first.native_handle(), buffer, 0, 64, &mapped) ==
        GRANIT_ERROR_UNSUPPORTED);
  CHECK(granit_buffer_map(second.native_handle(), buffer, 0, 64, &mapped) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_buffer_destroy(second.native_handle(), buffer) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_buffer_destroy(first.native_handle(), buffer) == GRANIT_SUCCESS);
}

TEST_CASE("C++ Buffer 提供 move-only RAII 并由 Renderer 级联失效", "[buffer][cpp_api]") {
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "granit-buffer-raii"});
  if (environment_unavailable(renderer_result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(renderer_result == granit::result::success);

  granit::buffer buffer;
  REQUIRE(
      buffer.initialize(renderer.native_handle(), {.size = 128,
                                                   .usage = granit::buffer_usage::transfer_source,
                                                   .location = granit::memory_location::upload}) ==
      granit::result::success);
  granit::buffer moved{std::move(buffer)};
  CHECK_FALSE(buffer.valid());
  REQUIRE(moved.valid());

  REQUIRE(renderer.reset() == granit::result::success);
  CHECK(moved.reset() == granit::result::invalid_handle);
  CHECK_FALSE(moved.valid());
}

TEST_CASE("初始数据和同步写入支持 UPLOAD 与 DEVICE Buffer", "[buffer][upload]") {
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "granit-buffer-upload"});
  if (environment_unavailable(renderer_result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(renderer_result == granit::result::success);

  std::array<std::byte, 256> data{};
  data[0] = std::byte{42};
  granit::buffer device_buffer;
  REQUIRE(device_buffer.initialize(renderer.native_handle(),
                                   {.size = data.size(),
                                    .usage = granit::buffer_usage::vertex,
                                    .location = granit::memory_location::device},
                                   data) == granit::result::success);
  data[1] = std::byte{24};
  REQUIRE(device_buffer.write(32, std::span<const std::byte>{data}.first(16)) ==
          granit::result::success);

  granit::buffer upload_buffer;
  REQUIRE(upload_buffer.initialize(renderer.native_handle(),
                                   {.size = data.size(),
                                    .usage = granit::buffer_usage::uniform,
                                    .location = granit::memory_location::upload},
                                   data) == granit::result::success);
  CHECK(upload_buffer.write(data.size(), std::span<const std::byte>{data}.first(1)) ==
        granit::result::invalid_argument);

  granit_buffer_desc readback_desc = GRANIT_BUFFER_DESC_INIT;
  readback_desc.size = data.size();
  readback_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  readback_desc.memory_location = GRANIT_MEMORY_LOCATION_READBACK;
  granit_buffer_initial_data initial_data = GRANIT_BUFFER_INITIAL_DATA_INIT;
  initial_data.data = data.data();
  initial_data.size = data.size();
  granit_buffer readback = GRANIT_NULL_HANDLE;
  CHECK(granit_buffer_create_with_data(renderer.native_handle(), &readback_desc, &initial_data,
                                       &readback) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(readback == GRANIT_NULL_HANDLE);
}

} // namespace
