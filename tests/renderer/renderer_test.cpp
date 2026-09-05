// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
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
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  REQUIRE(granit_renderer_get_status(renderer, &status) == GRANIT_SUCCESS);
  CHECK(status.state == GRANIT_RENDERER_STATE_READY);
  CHECK(status.failure_result == GRANIT_SUCCESS);
  CHECK(granit_renderer_process_events(renderer) == GRANIT_SUCCESS);
  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_get_status(renderer, &status) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_renderer_process_events(renderer) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_renderer_destroy(renderer) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Renderer 状态查询校验参数", "[renderer][status][c_api]") {
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  CHECK(granit_renderer_get_status(GRANIT_NULL_HANDLE, nullptr) == GRANIT_ERROR_INVALID_ARGUMENT);
  status.struct_size = GRANIT_RENDERER_STATUS_VERSION_1_SIZE - 1;
  CHECK(granit_renderer_get_status(GRANIT_NULL_HANDLE, &status) == GRANIT_ERROR_INVALID_ARGUMENT);
  status = GRANIT_RENDERER_STATUS_INIT;
  status.reserved = 1;
  CHECK(granit_renderer_get_status(GRANIT_NULL_HANDLE, &status) == GRANIT_ERROR_INVALID_ARGUMENT);
  status = GRANIT_RENDERER_STATUS_INIT;
  CHECK(granit_renderer_get_status(GRANIT_NULL_HANDLE, &status) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_renderer_process_events(GRANIT_NULL_HANDLE) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Renderer 公开查询设备限制", "[renderer][limits][c_api]") {
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
  CHECK((limits.framebuffer_sample_counts & GRANIT_SAMPLE_COUNT_1) != 0);
  CHECK(limits.max_sampler_anisotropy >= 1.0F);
  CHECK((limits.supported_features & GRANIT_RENDERER_FEATURE_TIMESTAMP_QUERY_BIT) != 0);

  limits.struct_size = GRANIT_RENDERER_LIMITS_VERSION_1_SIZE;
  limits.framebuffer_sample_counts = UINT32_MAX;
  limits.max_sampler_anisotropy = -1.0F;
  REQUIRE(granit_renderer_get_limits(renderer, &limits) == GRANIT_SUCCESS);
  CHECK((limits.framebuffer_sample_counts & GRANIT_SAMPLE_COUNT_1) != 0);
  CHECK(limits.max_sampler_anisotropy >= 1.0F);

  limits.struct_size = static_cast<std::uint32_t>(sizeof(granit_renderer_limits) + 64);
  REQUIRE(granit_renderer_get_limits(renderer, &limits) == GRANIT_SUCCESS);
  CHECK(limits.struct_size == sizeof(granit_renderer_limits) + 64);

  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_get_limits(renderer, &limits) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Renderer 公开查询 Shader 能力", "[renderer][shader-capabilities][c_api]") {
  granit_renderer_shader_capabilities capabilities = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
  CHECK(granit_renderer_get_shader_capabilities(GRANIT_NULL_HANDLE, nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  capabilities.struct_size = GRANIT_RENDERER_SHADER_CAPABILITIES_SIZE - 1;
  CHECK(granit_renderer_get_shader_capabilities(GRANIT_NULL_HANDLE, &capabilities) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  capabilities = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
  CHECK(granit_renderer_get_shader_capabilities(GRANIT_NULL_HANDLE, &capabilities) ==
        GRANIT_ERROR_INVALID_HANDLE);

  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.backend = GRANIT_RENDERER_BACKEND_VULKAN;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto create_result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(create_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(create_result == GRANIT_SUCCESS);

  capabilities = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
  capabilities.reserved = UINT32_MAX;
  CHECK(granit_renderer_get_shader_capabilities(renderer, &capabilities) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  capabilities = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
  REQUIRE(granit_renderer_get_shader_capabilities(renderer, &capabilities) == GRANIT_SUCCESS);
  CHECK(capabilities.backend == GRANIT_RENDERER_BACKEND_VULKAN);
  CHECK(capabilities.profile == GRANIT_SHADER_PROFILE_PORTABLE);
  CHECK(capabilities.supported_features == 0);

  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_get_shader_capabilities(renderer, &capabilities) ==
        GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Renderer 按设备能力选择 Shader 变体", "[renderer][shader-variant][c_api]") {
  std::uint32_t selected = 0;
  granit_shader_variant_requirement invalid = GRANIT_SHADER_VARIANT_REQUIREMENT_INIT;
  CHECK(granit_renderer_select_shader_variant(GRANIT_NULL_HANDLE, nullptr, 0, &selected) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(selected == UINT32_MAX);
  CHECK(granit_renderer_select_shader_variant(GRANIT_NULL_HANDLE, &invalid, 1, nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  invalid.struct_size = sizeof(invalid) - 1;
  CHECK(granit_renderer_select_shader_variant(GRANIT_NULL_HANDLE, &invalid, 1, &selected) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  invalid = GRANIT_SHADER_VARIANT_REQUIREMENT_INIT;
  CHECK(granit_renderer_select_shader_variant(GRANIT_NULL_HANDLE, &invalid, 1, &selected) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  invalid.backend = GRANIT_RENDERER_BACKEND_VULKAN;
  invalid.required_features = GRANIT_SHADER_FEATURE_ALL_BITS << 1;
  CHECK(granit_renderer_select_shader_variant(GRANIT_NULL_HANDLE, &invalid, 1, &selected) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.backend = GRANIT_RENDERER_BACKEND_VULKAN;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto create_result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(create_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(create_result == GRANIT_SUCCESS);

  std::array variants{
      granit_shader_variant_requirement{sizeof(granit_shader_variant_requirement),
                                        GRANIT_RENDERER_BACKEND_WEBGPU,
                                        GRANIT_SHADER_PROFILE_PORTABLE, 100, 0},
      granit_shader_variant_requirement{
          sizeof(granit_shader_variant_requirement), GRANIT_RENDERER_BACKEND_VULKAN,
          GRANIT_SHADER_PROFILE_PORTABLE, 20, GRANIT_SHADER_FEATURE_FLOAT16_BIT},
      granit_shader_variant_requirement{sizeof(granit_shader_variant_requirement),
                                        GRANIT_RENDERER_BACKEND_VULKAN,
                                        GRANIT_SHADER_PROFILE_PORTABLE, 10, 0},
      granit_shader_variant_requirement{sizeof(granit_shader_variant_requirement),
                                        GRANIT_RENDERER_BACKEND_VULKAN,
                                        GRANIT_SHADER_PROFILE_PORTABLE, 5, 0},
  };
  REQUIRE(granit_renderer_select_shader_variant(renderer, variants.data(),
                                                static_cast<std::uint32_t>(variants.size()),
                                                &selected) == GRANIT_SUCCESS);
  CHECK(selected == 2);

  variants[3].priority = variants[2].priority;
  REQUIRE(granit_renderer_select_shader_variant(renderer, variants.data(),
                                                static_cast<std::uint32_t>(variants.size()),
                                                &selected) == GRANIT_SUCCESS);
  CHECK(selected == 2);

  REQUIRE(granit_renderer_select_shader_variant(renderer, &variants[1], 1, &selected) ==
          GRANIT_ERROR_UNSUPPORTED);
  CHECK(selected == UINT32_MAX);
  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_select_shader_variant(renderer, variants.data(),
                                              static_cast<std::uint32_t>(variants.size()),
                                              &selected) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Renderer 查询实际后端与 Adapter 信息", "[renderer][info][c_api]") {
  granit_renderer_info info = GRANIT_RENDERER_INFO_INIT;
  CHECK(granit_renderer_get_info(GRANIT_NULL_HANDLE, nullptr) == GRANIT_ERROR_INVALID_ARGUMENT);
  info.struct_size = GRANIT_RENDERER_INFO_VERSION_1_SIZE - 1;
  CHECK(granit_renderer_get_info(GRANIT_NULL_HANDLE, &info) == GRANIT_ERROR_INVALID_ARGUMENT);
  info = GRANIT_RENDERER_INFO_INIT;
  CHECK(granit_renderer_get_info(GRANIT_NULL_HANDLE, &info) == GRANIT_ERROR_INVALID_HANDLE);

  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.backend = GRANIT_RENDERER_BACKEND_VULKAN;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto create_result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(create_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(create_result == GRANIT_SUCCESS);

  REQUIRE(granit_renderer_get_info(renderer, &info) == GRANIT_SUCCESS);
  CHECK(info.backend == GRANIT_RENDERER_BACKEND_VULKAN);
  CHECK(info.adapter_name_length > 0);
  std::vector<char> adapter_name(info.adapter_name_length + 1);
  info.adapter_name = adapter_name.data();
  info.adapter_name_capacity = static_cast<std::uint32_t>(adapter_name.size());
  REQUIRE(granit_renderer_get_info(renderer, &info) == GRANIT_SUCCESS);
  CHECK(adapter_name.back() == '\0');
  CHECK(adapter_name.front() != '\0');
  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
}

TEST_CASE("桌面 Renderer 明确拒绝 WebGPU", "[renderer][backend][webgpu]") {
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.backend = GRANIT_RENDERER_BACKEND_WEBGPU;
  diagnostic_messages captured;
  desc.diagnostic_callback = capture_diagnostic;
  desc.diagnostic_user_data = &captured;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_BACKEND_UNAVAILABLE);
  CHECK(renderer == GRANIT_NULL_HANDLE);
  REQUIRE(captured.categories.size() == 1);
  CHECK(captured.categories.front() == GRANIT_DIAGNOSTIC_CATEGORY_DEVICE);
  CHECK(captured.messages.front().find("WebGPU") != std::string::npos);
}

TEST_CASE("Renderer 资源统计覆盖创建销毁和无效句柄", "[renderer][resource-stats][c_api]") {
  granit_renderer_resource_stats stats = GRANIT_RENDERER_RESOURCE_STATS_INIT;
  CHECK(granit_renderer_get_resource_stats(GRANIT_NULL_HANDLE, nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  stats.struct_size = GRANIT_RENDERER_RESOURCE_STATS_VERSION_1_SIZE - 1;
  CHECK(granit_renderer_get_resource_stats(GRANIT_NULL_HANDLE, &stats) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  stats = GRANIT_RENDERER_RESOURCE_STATS_INIT;
  stats.reserved = 1;
  CHECK(granit_renderer_get_resource_stats(GRANIT_NULL_HANDLE, &stats) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  stats = GRANIT_RENDERER_RESOURCE_STATS_INIT;
  CHECK(granit_renderer_get_resource_stats(GRANIT_NULL_HANDLE, &stats) ==
        GRANIT_ERROR_INVALID_HANDLE);

  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto create_result = granit_renderer_create(&desc, &renderer);
  if (environment_unavailable(create_result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(create_result == GRANIT_SUCCESS);
  REQUIRE(granit_renderer_get_resource_stats(renderer, &stats) == GRANIT_SUCCESS);
  CHECK(stats.total_live_count == 0);

  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.size = 16;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer, &buffer_desc, &buffer) == GRANIT_SUCCESS);
  REQUIRE(granit_renderer_get_resource_stats(renderer, &stats) == GRANIT_SUCCESS);
  CHECK(stats.total_live_count == 1);
  CHECK(stats.buffer_count == 1);

  REQUIRE(granit_buffer_destroy(renderer, buffer) == GRANIT_SUCCESS);
  REQUIRE(granit_renderer_get_resource_stats(renderer, &stats) == GRANIT_SUCCESS);
  CHECK(stats.total_live_count == 0);
  CHECK(stats.buffer_count == 0);
  CHECK(stats.pending_retirement_count == 0);

  buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer, &buffer_desc, &buffer) == GRANIT_SUCCESS);
  const granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  granit_command_recorder recorder = GRANIT_NULL_HANDLE;
  REQUIRE(granit_command_recorder_create(renderer, &recorder_desc, &recorder) == GRANIT_SUCCESS);
  REQUIRE(granit_command_recorder_begin(renderer, recorder) == GRANIT_SUCCESS);
  REQUIRE(granit_command_recorder_fill_buffer(renderer, recorder, buffer, 0, 16, 0) ==
          GRANIT_SUCCESS);
  REQUIRE(granit_command_recorder_end(renderer, recorder) == GRANIT_SUCCESS);
  REQUIRE(granit_command_recorder_submit(renderer, recorder) == GRANIT_SUCCESS);
  REQUIRE(granit_buffer_destroy(renderer, buffer) == GRANIT_SUCCESS);
  REQUIRE(granit_renderer_get_resource_stats(renderer, &stats) == GRANIT_SUCCESS);
  CHECK(stats.buffer_count == 0);
  CHECK(stats.command_recorder_count == 1);
  CHECK(stats.pending_retirement_count >= 1);
  REQUIRE(granit_command_recorder_destroy(renderer, recorder) == GRANIT_SUCCESS);

  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_renderer_get_resource_stats(renderer, &stats) == GRANIT_ERROR_INVALID_HANDLE);
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

  desc = GRANIT_RENDERER_DESC_INIT;
  desc.backend = UINT32_MAX;
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

TEST_CASE("Renderer 拒绝旧版描述尺寸", "[renderer][validation]") {
  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.struct_size = GRANIT_RENDERER_DESC_SIZE - 1;

  granit_renderer renderer = GRANIT_NULL_HANDLE;
  CHECK(granit_renderer_create(&desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(renderer == GRANIT_NULL_HANDLE);
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
  CHECK(limits.supports_sample_count(granit::sample_count::one));
  CHECK(limits.supports_timestamp_queries());
  CHECK(limits.max_sampler_anisotropy >= 1.0F);
  CHECK(limits.uniform_buffer_offset_alignment > 0);
  granit::renderer_shader_capabilities shader_capabilities;
  REQUIRE(renderer.get_shader_capabilities(shader_capabilities) == granit::result::success);
  CHECK(shader_capabilities.backend == granit::renderer_backend::vulkan);
  CHECK(shader_capabilities.profile == GRANIT_SHADER_PROFILE_PORTABLE);
  CHECK_FALSE(shader_capabilities.supports(granit::shader_feature::float16));
  const std::array shader_variants{
      granit::shader_variant_requirement{.backend = granit::renderer_backend::webgpu,
                                         .priority = 100},
      granit::shader_variant_requirement{.backend = granit::renderer_backend::vulkan,
                                         .priority = 10},
  };
  const auto [variant_result, variant_index] = renderer.select_shader_variant(shader_variants);
  REQUIRE(variant_result == granit::result::success);
  CHECK(variant_index == 1);
  granit::renderer_resource_stats stats;
  REQUIRE(renderer.get_resource_stats(stats) == granit::result::success);
  CHECK(stats.total_live_count == 0);
  granit::renderer_info info;
  REQUIRE(renderer.get_info(info) == granit::result::success);
  CHECK(info.backend == granit::renderer_backend::vulkan);
  CHECK_FALSE(info.adapter_name.empty());
  CHECK(limits.max_uniform_buffer_binding_size > 0);

  granit::renderer_status status;
  REQUIRE(renderer.get_status(status) == granit::result::success);
  CHECK(status.state == granit::renderer_state::ready);
  CHECK(status.failure_result == granit::result::success);
  CHECK(renderer.process_events() == granit::result::success);

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
