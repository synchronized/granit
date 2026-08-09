// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_COMMAND_RECORDER_H_
#define GRANIT_BACKEND_VULKAN_COMMAND_RECORDER_H_

#include <granit/result.h>

#include <span>

#include <volk.h>

namespace granit::detail {

class vulkan_device;

enum class command_recorder_state { initial, recording, executable, pending, invalid };

/** 拥有独立 Command Pool 和主 Command Buffer 的一次性录制器。 */
class vulkan_command_recorder {
public:
  [[nodiscard]] granit_result initialize(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result begin(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result end(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result reset(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result copy_buffer(const vulkan_device& device, VkBuffer source,
                                          VkBuffer destination,
                                          std::span<const VkBufferCopy> regions) noexcept;
  [[nodiscard]] granit_result fill_buffer(const vulkan_device& device, VkBuffer buffer,
                                          VkDeviceSize offset, VkDeviceSize size,
                                          std::uint32_t value) noexcept;
  void destroy(const vulkan_device& device) noexcept;

  [[nodiscard]] command_recorder_state state() const noexcept { return state_; }
  [[nodiscard]] VkCommandBuffer native_handle() const noexcept { return command_buffer_; }

private:
  VkCommandPool pool_{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
  command_recorder_state state_{command_recorder_state::invalid};
};

} // namespace granit::detail

#endif
