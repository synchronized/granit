// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/device.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/loader.h"
#include "backend/vulkan/physical_device.h"
#include "backend/vulkan/result.h"

#include <utility>

#include <catch2/catch_all.hpp>

namespace {

using granit::detail::initialize_vulkan_loader;
using granit::detail::is_better_candidate;
using granit::detail::is_suitable;
using granit::detail::map_vulkan_result;
using granit::detail::physical_device_candidate;
using granit::detail::physical_device_kind;
using granit::detail::vulkan_device;
using granit::detail::vulkan_instance;
using granit::detail::vulkan_instance_desc;

TEST_CASE("Vulkan 结果映射为后端无关错误", "[vulkan][result]") {
  CHECK(map_vulkan_result(VK_SUCCESS) == GRANIT_SUCCESS);
  CHECK(map_vulkan_result(VK_ERROR_OUT_OF_HOST_MEMORY) == GRANIT_ERROR_OUT_OF_MEMORY);
  CHECK(map_vulkan_result(VK_ERROR_OUT_OF_DEVICE_MEMORY) == GRANIT_ERROR_OUT_OF_MEMORY);
  CHECK(map_vulkan_result(VK_ERROR_DEVICE_LOST) == GRANIT_ERROR_DEVICE_LOST);
  CHECK(map_vulkan_result(VK_ERROR_LAYER_NOT_PRESENT) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(map_vulkan_result(VK_ERROR_EXTENSION_NOT_PRESENT) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(map_vulkan_result(VK_ERROR_INCOMPATIBLE_DRIVER) == GRANIT_ERROR_INCOMPATIBLE_DRIVER);
  CHECK(map_vulkan_result(VK_ERROR_INITIALIZATION_FAILED) == GRANIT_ERROR_INITIALIZATION_FAILED);
  CHECK(map_vulkan_result(VK_ERROR_UNKNOWN) == GRANIT_ERROR_UNKNOWN);
  CHECK(map_vulkan_result(VK_TIMEOUT) == GRANIT_ERROR_INTERNAL);
}

TEST_CASE("Vulkan loader 至少支持 1.3", "[vulkan][loader]") {
  const auto status = initialize_vulkan_loader();
  if (status.result == GRANIT_ERROR_BACKEND_UNAVAILABLE ||
      status.result == GRANIT_ERROR_INCOMPATIBLE_DRIVER) {
    SKIP("当前运行环境没有可用的 Vulkan 1.3 loader");
  }

  REQUIRE(status.result == GRANIT_SUCCESS);
  CHECK(status.api_version >= VK_API_VERSION_1_3);
}

TEST_CASE("无窗口 Vulkan instance 支持移动和显式重置", "[vulkan][instance]") {
  const auto loader = initialize_vulkan_loader();
  if (loader.result != GRANIT_SUCCESS) {
    SKIP("当前运行环境没有可用的 Vulkan 1.3 loader");
  }

  vulkan_instance instance;
  REQUIRE(
    instance.initialize(vulkan_instance_desc{.application_name = "granit-tests"}) ==
    GRANIT_SUCCESS);
  REQUIRE(instance.valid());
  REQUIRE(instance.native_handle() != VK_NULL_HANDLE);

  vulkan_instance moved{std::move(instance)};
  CHECK_FALSE(instance.valid());
  CHECK(moved.valid());

  moved.reset();
  CHECK_FALSE(moved.valid());
}

TEST_CASE("Vulkan instance 拒绝无效描述和重复初始化", "[vulkan][instance]") {
  vulkan_instance instance;
  CHECK(instance.initialize({}) == GRANIT_ERROR_INVALID_ARGUMENT);

  const auto loader = initialize_vulkan_loader();
  if (loader.result != GRANIT_SUCCESS) {
    SKIP("当前运行环境没有可用的 Vulkan 1.3 loader");
  }

  REQUIRE(instance.initialize({.application_name = "granit-tests"}) == GRANIT_SUCCESS);
  CHECK(
    instance.initialize({.application_name = "granit-tests"}) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Vulkan instance 可选启用验证层", "[vulkan][validation]") {
  const auto loader = initialize_vulkan_loader();
  if (loader.result != GRANIT_SUCCESS) {
    SKIP("当前运行环境没有可用的 Vulkan 1.3 loader");
  }

  vulkan_instance instance;
  const auto result = instance.initialize(
    {.application_name = "granit-validation-tests", .enable_validation = true});
  if (result == GRANIT_ERROR_UNSUPPORTED) {
    SKIP("当前运行环境没有 Khronos validation layer 或 debug utils extension");
  }
  REQUIRE(result == GRANIT_SUCCESS);
  CHECK(instance.valid());
}

TEST_CASE("物理设备必须满足 Vulkan 1.3 基础能力", "[vulkan][device_selection]") {
  physical_device_candidate candidate{
    .kind = physical_device_kind::integrated_gpu,
    .api_version = VK_API_VERSION_1_3,
    .has_graphics_queue = true,
    .dynamic_rendering = true,
    .synchronization2 = true,
    .maintenance4 = true,
  };
  CHECK(is_suitable(candidate));

  candidate.api_version = VK_API_VERSION_1_2;
  CHECK_FALSE(is_suitable(candidate));
  candidate.api_version = VK_API_VERSION_1_3;
  candidate.dynamic_rendering = false;
  CHECK_FALSE(is_suitable(candidate));
}

TEST_CASE("设备选择优先类型、显存和枚举顺序", "[vulkan][device_selection]") {
  const physical_device_candidate integrated{
    .kind = physical_device_kind::integrated_gpu,
    .api_version = VK_API_VERSION_1_3,
    .device_local_memory = UINT64_C(16) << 30,
    .enumeration_index = 0,
    .has_graphics_queue = true,
    .dynamic_rendering = true,
    .synchronization2 = true,
    .maintenance4 = true,
  };
  auto discrete = integrated;
  discrete.kind = physical_device_kind::discrete_gpu;
  discrete.device_local_memory = UINT64_C(8) << 30;
  discrete.enumeration_index = 1;
  CHECK(is_better_candidate(discrete, integrated));

  auto larger_discrete = discrete;
  larger_discrete.device_local_memory = UINT64_C(12) << 30;
  larger_discrete.enumeration_index = 2;
  CHECK(is_better_candidate(larger_discrete, discrete));

  auto same_device = larger_discrete;
  same_device.enumeration_index = 3;
  CHECK_FALSE(is_better_candidate(same_device, larger_discrete));
}

TEST_CASE("创建带独立函数表的 Vulkan 逻辑设备", "[vulkan][device]") {
  const auto loader = initialize_vulkan_loader();
  if (loader.result != GRANIT_SUCCESS) {
    SKIP("当前运行环境没有可用的 Vulkan 1.3 loader");
  }

  vulkan_instance instance;
  REQUIRE(instance.initialize({.application_name = "granit-device-tests"}) == GRANIT_SUCCESS);

  vulkan_device device;
  const auto result = device.initialize(instance);
  if (result == GRANIT_ERROR_NO_SUITABLE_DEVICE) {
    SKIP("当前运行环境没有满足 Granit Vulkan 1.3 要求的图形设备");
  }
  REQUIRE(result == GRANIT_SUCCESS);
  REQUIRE(device.valid());
  CHECK(device.physical_device() != VK_NULL_HANDLE);
  CHECK(device.graphics_queue() != VK_NULL_HANDLE);
  CHECK(device.functions().vkQueueSubmit2 != nullptr);

  vulkan_device moved{std::move(device)};
  CHECK_FALSE(device.valid());
  CHECK(moved.valid());
  moved.reset();
  CHECK_FALSE(moved.valid());
}

} // namespace
