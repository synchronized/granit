// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/device.h"
#include "backend/vulkan/frame_context.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/loader.h"
#include "backend/vulkan/memory_allocator.h"
#include "backend/vulkan/physical_device.h"
#include "backend/vulkan/result.h"
#include "backend/vulkan/swapchain.h"

#include <utility>

#include <catch2/catch_all.hpp>

namespace {

using granit::detail::initialize_vulkan_loader;
using granit::detail::is_better_candidate;
using granit::detail::is_suitable;
using granit::detail::map_vulkan_result;
using granit::detail::physical_device_candidate;
using granit::detail::physical_device_kind;
using granit::detail::vulkan_buffer_allocation;
using granit::detail::vulkan_device;
using granit::detail::vulkan_frame_context;
using granit::detail::vulkan_image_allocation;
using granit::detail::vulkan_instance;
using granit::detail::vulkan_instance_desc;
using granit::detail::vulkan_memory_allocator;
using granit::detail::vulkan_memory_location;

TEST_CASE("Vulkan 结果映射为后端无关错误", "[vulkan][result]") {
  CHECK(map_vulkan_result(VK_SUCCESS) == GRANIT_SUCCESS);
  CHECK(map_vulkan_result(VK_ERROR_OUT_OF_HOST_MEMORY) == GRANIT_ERROR_OUT_OF_MEMORY);
  CHECK(map_vulkan_result(VK_ERROR_OUT_OF_DEVICE_MEMORY) == GRANIT_ERROR_OUT_OF_MEMORY);
  CHECK(map_vulkan_result(VK_ERROR_DEVICE_LOST) == GRANIT_ERROR_DEVICE_LOST);
  CHECK(map_vulkan_result(VK_ERROR_LAYER_NOT_PRESENT) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(map_vulkan_result(VK_ERROR_EXTENSION_NOT_PRESENT) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(map_vulkan_result(VK_ERROR_INCOMPATIBLE_DRIVER) == GRANIT_ERROR_INCOMPATIBLE_DRIVER);
  CHECK(map_vulkan_result(VK_ERROR_INITIALIZATION_FAILED) == GRANIT_ERROR_INITIALIZATION_FAILED);
  CHECK(map_vulkan_result(VK_ERROR_SURFACE_LOST_KHR) == GRANIT_ERROR_SURFACE_LOST);
  CHECK(map_vulkan_result(VK_ERROR_OUT_OF_DATE_KHR) == GRANIT_ERROR_OUT_OF_DATE);
  CHECK(map_vulkan_result(VK_NOT_READY) == GRANIT_ERROR_NOT_READY);
  CHECK(map_vulkan_result(VK_TIMEOUT) == GRANIT_ERROR_NOT_READY);
  CHECK(map_vulkan_result(VK_ERROR_UNKNOWN) == GRANIT_ERROR_UNKNOWN);
}

TEST_CASE("Swapchain acquire 和 present 拒绝无效后端状态", "[vulkan][swapchain][frame]") {
  granit::detail::vulkan_swapchain swapchain;
  vulkan_device device;
  CHECK(swapchain.acquire(device, VK_NULL_HANDLE).result == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(swapchain.present(device, VK_NULL_HANDLE, 0, VK_NULL_HANDLE).result ==
        GRANIT_ERROR_INVALID_ARGUMENT);
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
  REQUIRE(instance.initialize(vulkan_instance_desc{.application_name = "granit-tests"}) ==
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
  CHECK(instance.initialize({.application_name = "granit-tests"}) == GRANIT_ERROR_INVALID_ARGUMENT);
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
  candidate.dynamic_rendering = true;
  candidate.supports_requested_surfaces = false;
  CHECK_FALSE(is_suitable(candidate));
  candidate.supports_requested_surfaces = true;
  candidate.supports_swapchain = false;
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

TEST_CASE("每帧上下文创建初始已触发 Fence 和两个二进制 Semaphore", "[vulkan][frame]") {
  const auto loader = initialize_vulkan_loader();
  if (loader.result != GRANIT_SUCCESS) {
    SKIP("当前运行环境没有可用的 Vulkan 1.3 loader");
  }

  vulkan_instance instance;
  REQUIRE(instance.initialize({.application_name = "granit-frame-tests"}) == GRANIT_SUCCESS);
  vulkan_device device;
  const auto device_result = device.initialize(instance);
  if (device_result == GRANIT_ERROR_NO_SUITABLE_DEVICE) {
    SKIP("当前运行环境没有满足 Granit Vulkan 1.3 要求的图形设备");
  }
  REQUIRE(device_result == GRANIT_SUCCESS);

  vulkan_frame_context frame;
  REQUIRE(frame.initialize(device) == GRANIT_SUCCESS);
  CHECK(frame.valid());
  CHECK(frame.completion_fence() != VK_NULL_HANDLE);
  CHECK(frame.image_available() != VK_NULL_HANDLE);
  CHECK(frame.render_finished() != VK_NULL_HANDLE);
  CHECK(frame.wait(device, 0) == GRANIT_SUCCESS);
  CHECK(frame.initialize(device) == GRANIT_ERROR_INVALID_ARGUMENT);

  frame.destroy(device);
  CHECK_FALSE(frame.valid());
}

TEST_CASE("VMA 分配并释放内部 Buffer 和 Image", "[vulkan][memory]") {
  const auto loader = initialize_vulkan_loader();
  if (loader.result != GRANIT_SUCCESS) {
    SKIP("当前运行环境没有可用的 Vulkan 1.3 loader");
  }

  vulkan_instance instance;
  REQUIRE(instance.initialize({.application_name = "granit-memory-tests"}) == GRANIT_SUCCESS);
  vulkan_device device;
  const auto device_result = device.initialize(instance);
  if (device_result == GRANIT_ERROR_NO_SUITABLE_DEVICE) {
    SKIP("当前运行环境没有满足 Granit Vulkan 1.3 要求的图形设备");
  }
  REQUIRE(device_result == GRANIT_SUCCESS);

  vulkan_memory_allocator allocator;
  REQUIRE(allocator.initialize(instance, device) == GRANIT_SUCCESS);
  REQUIRE(allocator.valid());

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = 4096;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vulkan_buffer_allocation buffer;
  REQUIRE(allocator.create_buffer(buffer_info, vulkan_memory_location::upload, buffer) ==
          GRANIT_SUCCESS);
  REQUIRE(buffer.buffer != VK_NULL_HANDLE);
  REQUIRE(buffer.mapped_data != nullptr);
  static_cast<unsigned char*>(buffer.mapped_data)[0] = 42;
  CHECK(allocator.flush(buffer, 0, 1) == GRANIT_SUCCESS);
  allocator.destroy_buffer(buffer);
  CHECK(buffer.buffer == VK_NULL_HANDLE);

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.extent = {.width = 4, .height = 4, .depth = 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vulkan_image_allocation image;
  REQUIRE(allocator.create_image(image_info, vulkan_memory_location::device, image) ==
          GRANIT_SUCCESS);
  REQUIRE(image.image != VK_NULL_HANDLE);
  allocator.destroy_image(image);
  CHECK(image.image == VK_NULL_HANDLE);
}

} // namespace
