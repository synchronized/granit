// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_FRAME_CONTEXT_H_
#define GRANIT_BACKEND_VULKAN_FRAME_CONTEXT_H_

#include <cstdint>

#include <granit/result.h>

#include <volk.h>

namespace granit::detail {

class vulkan_device;

/** 拥有一个 Vulkan Fence；仅供 Renderer 内部提交调度使用。 */
class vulkan_fence {
public:
  vulkan_fence() = default;
  ~vulkan_fence() = default;

  vulkan_fence(const vulkan_fence&) = delete;
  vulkan_fence& operator=(const vulkan_fence&) = delete;
  vulkan_fence(vulkan_fence&&) = delete;
  vulkan_fence& operator=(vulkan_fence&&) = delete;

  [[nodiscard]] granit_result initialize(const vulkan_device& device, bool signaled) noexcept;
  [[nodiscard]] granit_result wait(const vulkan_device& device, std::uint64_t timeout) noexcept;
  [[nodiscard]] granit_result reset(const vulkan_device& device) noexcept;
  void destroy(const vulkan_device& device) noexcept;

  [[nodiscard]] bool valid() const noexcept { return handle_ != VK_NULL_HANDLE; }
  [[nodiscard]] VkFence native_handle() const noexcept { return handle_; }

private:
  VkFence handle_{VK_NULL_HANDLE};
};

/** 拥有一个二进制 Vulkan Semaphore。 */
class vulkan_binary_semaphore {
public:
  vulkan_binary_semaphore() = default;
  ~vulkan_binary_semaphore() = default;

  vulkan_binary_semaphore(const vulkan_binary_semaphore&) = delete;
  vulkan_binary_semaphore& operator=(const vulkan_binary_semaphore&) = delete;
  vulkan_binary_semaphore(vulkan_binary_semaphore&&) = delete;
  vulkan_binary_semaphore& operator=(vulkan_binary_semaphore&&) = delete;

  [[nodiscard]] granit_result initialize(const vulkan_device& device) noexcept;
  void destroy(const vulkan_device& device) noexcept;

  [[nodiscard]] bool valid() const noexcept { return handle_ != VK_NULL_HANDLE; }
  [[nodiscard]] VkSemaphore native_handle() const noexcept { return handle_; }

private:
  VkSemaphore handle_{VK_NULL_HANDLE};
};

/** 单个 frames-in-flight 槽位的同步对象集合。 */
class vulkan_frame_context {
public:
  [[nodiscard]] granit_result initialize(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result wait(const vulkan_device& device, std::uint64_t timeout) noexcept;
  [[nodiscard]] granit_result reset_fence(const vulkan_device& device) noexcept;
  void destroy(const vulkan_device& device) noexcept;

  [[nodiscard]] bool valid() const noexcept {
    return completion_fence_.valid() && image_available_.valid() && render_finished_.valid();
  }
  [[nodiscard]] VkFence completion_fence() const noexcept {
    return completion_fence_.native_handle();
  }
  [[nodiscard]] VkSemaphore image_available() const noexcept {
    return image_available_.native_handle();
  }
  [[nodiscard]] VkSemaphore render_finished() const noexcept {
    return render_finished_.native_handle();
  }

private:
  vulkan_fence completion_fence_;
  vulkan_binary_semaphore image_available_;
  vulkan_binary_semaphore render_finished_;
};

} // namespace granit::detail

#endif
