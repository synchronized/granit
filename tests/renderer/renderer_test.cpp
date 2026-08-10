// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.h>
#include <granit/renderer/renderer.hpp>

#include <utility>

#include <catch2/catch_all.hpp>

namespace {

bool environment_unavailable(granit_result result) {
  return result == GRANIT_ERROR_BACKEND_UNAVAILABLE || result == GRANIT_ERROR_INCOMPATIBLE_DRIVER ||
         result == GRANIT_ERROR_NO_SUITABLE_DEVICE;
}

TEST_CASE("C API 创建并销毁真实 renderer", "[renderer][c_api]") {
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  constexpr char application_name[] = "granit-c-api-tests";
  desc.application_name = application_name;
  desc.application_name_length = static_cast<std::uint32_t>(sizeof(application_name) - 1);

  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == GRANIT_SUCCESS);
  REQUIRE(renderer != GRANIT_NULL_HANDLE);
  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_destroy(renderer) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Renderer 描述拒绝未知字段和非法字符串", "[renderer][validation]") {
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;

  desc.flags = UINT32_C(0x80000000);
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);

  desc = GRANIT_RENDERER_DESC_INIT;
  desc.surface_types = UINT32_C(0x80000000);
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);

  desc = GRANIT_RENDERER_DESC_INIT;
  desc.frames_in_flight = 0;
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);

  desc = GRANIT_RENDERER_DESC_INIT;
  desc.frames_in_flight = GRANIT_MAX_FRAMES_IN_FLIGHT + 1;
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);

  desc = GRANIT_RENDERER_DESC_INIT;
  desc.reserved = 1;
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);

  desc = GRANIT_RENDERER_DESC_INIT;
  desc.application_name = "invalid";
  desc.application_name_length = 0;
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);

  constexpr char embedded_zero[] = {'a', '\0', 'b'};
  desc.application_name = embedded_zero;
  desc.application_name_length = static_cast<std::uint32_t>(sizeof(embedded_zero));
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Renderer 接受不含 Surface 字段的旧描述尺寸", "[renderer][compatibility]") {
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.struct_size = GRANIT_RENDERER_DESC_VERSION_1_SIZE;
  desc.surface_types = UINT32_C(0x80000000);

  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == GRANIT_SUCCESS);
  CHECK(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
}

TEST_CASE("Renderer 旧版描述不读取 frames-in-flight 扩展字段", "[renderer][compatibility]") {
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.struct_size = GRANIT_RENDERER_DESC_VERSION_2_SIZE;
  desc.frames_in_flight = 0;
  desc.reserved = UINT32_MAX;

  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == GRANIT_SUCCESS);
  CHECK(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
}

TEST_CASE("C++ renderer 提供 move-only RAII", "[renderer][cpp_api]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-cpp-tests"});
  if (environment_unavailable(granit::to_native(result))) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == granit::result::success);
  REQUIRE(renderer.valid());

  granit::renderer moved{std::move(renderer)};
  CHECK_FALSE(renderer.valid());
  CHECK(moved.valid());
  CHECK(moved.reset() == granit::result::success);
  CHECK_FALSE(moved.valid());
}

TEST_CASE("验证模式在活动资源存在时仍完成 Renderer 级联销毁", "[renderer][lifecycle]") {
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.flags = GRANIT_RENDERER_ENABLE_VALIDATION_BIT;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto create_result = granit_renderer_create(&renderer_desc, &renderer);
  if (create_result == GRANIT_ERROR_UNSUPPORTED || environment_unavailable(create_result)) {
    SKIP("当前运行环境不支持 Vulkan 验证层或没有满足要求的设备");
  }
  REQUIRE(create_result == GRANIT_SUCCESS);

  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.size = 16;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer, &buffer_desc, &buffer) == GRANIT_SUCCESS);

  CHECK(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_buffer_destroy(renderer, buffer) == GRANIT_ERROR_INVALID_HANDLE);
}

} // namespace
