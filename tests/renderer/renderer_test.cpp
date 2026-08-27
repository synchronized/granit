// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.h>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/upload_batch.h>

#include <array>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_all.hpp>

namespace {

bool environment_unavailable(granit_result result) {
  return result == GRANIT_ERROR_BACKEND_UNAVAILABLE || result == GRANIT_ERROR_INCOMPATIBLE_DRIVER ||
         result == GRANIT_ERROR_NO_SUITABLE_DEVICE;
}

struct diagnostic_messages {
  std::mutex mutex;
  std::vector<granit_diagnostic_category> categories;
  std::vector<std::string> messages;
};

void capture_diagnostic(granit_diagnostic_severity, granit_diagnostic_category category,
                        const char* message, std::uint32_t message_length, void* user_data) {
  auto& captured = *static_cast<diagnostic_messages*>(user_data);
  std::lock_guard lock{captured.mutex};
  captured.categories.push_back(category);
  captured.messages.emplace_back(message, message_length);
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

TEST_CASE("Renderer 公开查询 Uniform Buffer 限制", "[renderer][limits][c_api]") {
  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  CHECK(granit_renderer_get_limits(GRANIT_NULL_HANDLE, nullptr) == GRANIT_ERROR_INVALID_ARGUMENT);

  limits.struct_size = GRANIT_RENDERER_LIMITS_VERSION_1_SIZE - 1;
  CHECK(granit_renderer_get_limits(GRANIT_NULL_HANDLE, &limits) == GRANIT_ERROR_INVALID_ARGUMENT);

  limits = GRANIT_RENDERER_LIMITS_INIT;
  CHECK(granit_renderer_get_limits(GRANIT_NULL_HANDLE, &limits) == GRANIT_ERROR_INVALID_HANDLE);

  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto create_result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(create_result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(create_result == GRANIT_SUCCESS);

  limits = GRANIT_RENDERER_LIMITS_INIT;
  limits.reserved = UINT32_MAX;
  REQUIRE(granit_renderer_get_limits(renderer, &limits) == GRANIT_SUCCESS);
  CHECK(limits.struct_size == sizeof(granit_renderer_limits));
  CHECK(limits.reserved == 0);
  CHECK(limits.uniform_buffer_offset_alignment > 0);
  CHECK(limits.max_uniform_buffer_binding_size > 0);

  limits.struct_size = static_cast<std::uint32_t>(sizeof(granit_renderer_limits) + 64);
  REQUIRE(granit_renderer_get_limits(renderer, &limits) == GRANIT_SUCCESS);
  CHECK(limits.struct_size == sizeof(granit_renderer_limits) + 64);

  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_get_limits(renderer, &limits) == GRANIT_ERROR_INVALID_HANDLE);
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

  desc = GRANIT_RENDERER_DESC_INIT;
  desc.diagnostic_user_data = &renderer;
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);

  CHECK(granit_renderer_set_object_name(GRANIT_NULL_HANDLE, UINT64_C(1), "buffer", 6) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_renderer_set_object_name(UINT64_C(1), UINT64_C(2), nullptr, 0) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Renderer 为 GPU 资源设置调试名称", "[renderer][diagnostic][object-name]") {
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.flags = GRANIT_RENDERER_ENABLE_VALIDATION_BIT;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto create_result = granit_renderer_create(&renderer_desc, &renderer);
  if (create_result == GRANIT_ERROR_UNSUPPORTED || environment_unavailable(create_result))
    SKIP("当前运行环境不支持 Vulkan 验证层或没有满足要求的设备");
  REQUIRE(create_result == GRANIT_SUCCESS);

  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.size = 16;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer, &buffer_desc, &buffer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_set_object_name(renderer, buffer, "upload-buffer", 13) == GRANIT_SUCCESS);
  REQUIRE(granit_buffer_destroy(renderer, buffer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_set_object_name(renderer, buffer, "stale", 5) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
}

TEST_CASE("对象调试名称拒绝跨 Renderer 和纯管理句柄", "[renderer][diagnostic][object-name]") {
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.flags = GRANIT_RENDERER_ENABLE_VALIDATION_BIT;
  granit_renderer first = GRANIT_NULL_HANDLE;
  granit_renderer second = GRANIT_NULL_HANDLE;
  const auto first_result = granit_renderer_create(&desc, &first);
  if (first_result == GRANIT_ERROR_UNSUPPORTED || environment_unavailable(first_result))
    SKIP("当前运行环境不支持 Vulkan 验证层或没有满足要求的设备");
  REQUIRE(first_result == GRANIT_SUCCESS);
  REQUIRE(granit_renderer_create(&desc, &second) == GRANIT_SUCCESS);

  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.size = 16;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(first, &buffer_desc, &buffer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_set_object_name(second, buffer, "foreign", 7) ==
        GRANIT_ERROR_INVALID_HANDLE);

  granit_upload_batch batch = GRANIT_NULL_HANDLE;
  const granit_upload_batch_desc batch_desc = GRANIT_UPLOAD_BATCH_DESC_INIT;
  REQUIRE(granit_upload_batch_create(first, &batch_desc, &batch) == GRANIT_SUCCESS);
  CHECK(granit_renderer_set_object_name(first, batch, "batch", 5) == GRANIT_ERROR_UNSUPPORTED);
  REQUIRE(granit_upload_batch_destroy(first, batch) == GRANIT_SUCCESS);
  REQUIRE(granit_buffer_destroy(first, buffer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_destroy(second) == GRANIT_SUCCESS);
  CHECK(granit_renderer_destroy(first) == GRANIT_SUCCESS);
}

TEST_CASE("未启用验证时对象调试名称明确降级", "[renderer][diagnostic][object-name]") {
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto create_result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(create_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(create_result == GRANIT_SUCCESS);

  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.size = 16;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer, &buffer_desc, &buffer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_set_object_name(renderer, buffer, "buffer", 6) == GRANIT_ERROR_UNSUPPORTED);
  REQUIRE(granit_buffer_destroy(renderer, buffer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
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
  CHECK(renderer.import_pipeline_cache({}) == granit::result::success);
  REQUIRE(renderer.valid());

  granit::renderer_limits limits;
  REQUIRE(renderer.get_limits(limits) == granit::result::success);
  CHECK(limits.uniform_buffer_offset_alignment > 0);
  CHECK(limits.max_uniform_buffer_binding_size > 0);

  granit::renderer moved{std::move(renderer)};
  CHECK_FALSE(renderer.valid());
  CHECK(moved.valid());
  CHECK(moved.reset() == granit::result::success);
  CHECK_FALSE(moved.valid());
}

TEST_CASE("Renderer Pipeline Cache 支持导出和重新导入", "[renderer][pipeline-cache]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-pipeline-cache-tests"});
  if (environment_unavailable(granit::to_native(result)))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  std::uint64_t size{};
  REQUIRE(renderer.query_pipeline_cache_size(size) == granit::result::success);
  REQUIRE(size > 0);
  std::vector<std::byte> cache(static_cast<std::size_t>(size));
  REQUIRE(renderer.export_pipeline_cache(cache, size) == granit::result::success);
  REQUIRE(size > 0);
  cache.resize(static_cast<std::size_t>(size));
  REQUIRE(renderer.import_pipeline_cache(cache) == granit::result::success);

  const std::array invalid_cache{std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3}};
  CHECK(renderer.import_pipeline_cache(invalid_cache) == granit::result::invalid_argument);
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

TEST_CASE("Renderer 诊断回调接收生命周期消息", "[renderer][diagnostic]") {
  diagnostic_messages captured;
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.flags = GRANIT_RENDERER_ENABLE_VALIDATION_BIT;
  renderer_desc.diagnostic_callback = capture_diagnostic;
  renderer_desc.diagnostic_user_data = &captured;
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
  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);

  std::lock_guard lock{captured.mutex};
  bool found_lifecycle_message = false;
  for (std::size_t index = 0; index < captured.messages.size(); ++index) {
    if (captured.categories[index] == GRANIT_DIAGNOSTIC_CATEGORY_LIFECYCLE &&
        captured.messages[index].find("Buffer=1") != std::string::npos) {
      found_lifecycle_message = true;
      break;
    }
  }
  CHECK(found_lifecycle_message);
}

TEST_CASE("Renderer 诊断回调定位参数错误和跨 Renderer 句柄", "[renderer][diagnostic]") {
  diagnostic_messages captured;
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.diagnostic_callback = capture_diagnostic;
  renderer_desc.diagnostic_user_data = &captured;
  granit_renderer first = GRANIT_NULL_HANDLE;
  const auto first_result = granit_renderer_create(&renderer_desc, &first);
  if (environment_unavailable(first_result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(first_result == GRANIT_SUCCESS);

  granit_renderer second = GRANIT_NULL_HANDLE;
  REQUIRE(granit_renderer_create(&renderer_desc, &second) == GRANIT_SUCCESS);

  granit_buffer_desc invalid_desc = GRANIT_BUFFER_DESC_INIT;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  CHECK(granit_buffer_create(first, &invalid_desc, &buffer) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit_buffer_desc valid_desc = GRANIT_BUFFER_DESC_INIT;
  valid_desc.size = 16;
  valid_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  REQUIRE(granit_buffer_create(first, &valid_desc, &buffer) == GRANIT_SUCCESS);
  CHECK(granit_buffer_destroy(second, buffer) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_buffer_destroy(first, buffer) == GRANIT_SUCCESS);
  REQUIRE(granit_renderer_destroy(second) == GRANIT_SUCCESS);
  REQUIRE(granit_renderer_destroy(first) == GRANIT_SUCCESS);

  std::lock_guard lock{captured.mutex};
  bool found_desc = false;
  bool found_handle = false;
  for (std::size_t index = 0; index < captured.messages.size(); ++index) {
    if (captured.categories[index] != GRANIT_DIAGNOSTIC_CATEGORY_VALIDATION)
      continue;
    found_desc |= captured.messages[index].find("granit_buffer_create") != std::string::npos &&
                  captured.messages[index].find("desc") != std::string::npos;
    found_handle |= captured.messages[index].find("granit_buffer_destroy") != std::string::npos &&
                    captured.messages[index].find("Buffer") != std::string::npos &&
                    captured.messages[index].find("Renderer") != std::string::npos;
  }
  CHECK(found_desc);
  CHECK(found_handle);
}

} // namespace
