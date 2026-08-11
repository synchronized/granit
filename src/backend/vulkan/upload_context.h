// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_UPLOAD_CONTEXT_H_
#define GRANIT_BACKEND_VULKAN_UPLOAD_CONTEXT_H_

#include <granit/core/result.h>

#include "backend/vulkan/frame_context.h"
#include "backend/vulkan/memory_allocator.h"

namespace granit::detail {

class vulkan_device;

/** 可重复使用的同步上传对象集合。外部必须保证同一时刻只有一个线程使用。 */
class vulkan_upload_context {
public:
  [[nodiscard]] granit_result initialize(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result ensure_capacity(vulkan_memory_allocator& allocator,
                                              VkDeviceSize required) noexcept;
  [[nodiscard]] granit_result begin(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result end(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result reset_fence(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result wait(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result restore_signaled_fence(const vulkan_device& device) noexcept;
  void destroy(const vulkan_device& device, vulkan_memory_allocator& allocator) noexcept;

  [[nodiscard]] VkCommandBuffer command_buffer() const noexcept { return command_buffer_; }
  [[nodiscard]] VkFence fence() const noexcept { return fence_.native_handle(); }
  [[nodiscard]] const vulkan_buffer_allocation& staging() const noexcept { return staging_; }

private:
  VkCommandPool command_pool_{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
  vulkan_fence fence_;
  vulkan_buffer_allocation staging_;
  VkDeviceSize capacity_{};
};

} // namespace granit::detail

#endif
