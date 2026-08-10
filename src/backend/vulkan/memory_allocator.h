// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_MEMORY_ALLOCATOR_H_
#define GRANIT_BACKEND_VULKAN_MEMORY_ALLOCATOR_H_

#include <granit/core/result.h>

#include <volk.h>
#include <vk_mem_alloc.h>

namespace granit::detail {

class vulkan_device;
class vulkan_instance;

enum class vulkan_memory_location { automatic, device, upload, readback };

struct vulkan_buffer_allocation {
  VkBuffer buffer{VK_NULL_HANDLE};
  VmaAllocation allocation{};
  void* mapped_data{};
};

struct vulkan_image_allocation {
  VkImage image{VK_NULL_HANDLE};
  VmaAllocation allocation{};
};

/** 管理单个 Renderer 的 Vulkan 资源内存。 */
class vulkan_memory_allocator {
public:
  vulkan_memory_allocator() = default;
  ~vulkan_memory_allocator();

  vulkan_memory_allocator(const vulkan_memory_allocator&) = delete;
  vulkan_memory_allocator& operator=(const vulkan_memory_allocator&) = delete;
  vulkan_memory_allocator(vulkan_memory_allocator&& other) noexcept;
  vulkan_memory_allocator& operator=(vulkan_memory_allocator&& other) noexcept;

  [[nodiscard]] granit_result initialize(const vulkan_instance& instance,
                                         const vulkan_device& device) noexcept;
  void reset() noexcept;

  [[nodiscard]] bool valid() const noexcept { return allocator_ != VK_NULL_HANDLE; }
  [[nodiscard]] granit_result create_buffer(const VkBufferCreateInfo& create_info,
                                            vulkan_memory_location location,
                                            vulkan_buffer_allocation& buffer) noexcept;
  void destroy_buffer(vulkan_buffer_allocation& buffer) noexcept;
  [[nodiscard]] granit_result create_image(const VkImageCreateInfo& create_info,
                                           vulkan_memory_location location,
                                           vulkan_image_allocation& image) noexcept;
  void destroy_image(vulkan_image_allocation& image) noexcept;
  [[nodiscard]] granit_result flush(const vulkan_buffer_allocation& buffer, VkDeviceSize offset,
                                    VkDeviceSize size) noexcept;
  [[nodiscard]] granit_result invalidate(const vulkan_buffer_allocation& buffer,
                                         VkDeviceSize offset, VkDeviceSize size) noexcept;

private:
  VmaAllocator allocator_{};
};

} // namespace granit::detail

#endif
