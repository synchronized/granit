// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_BINDLESS_DESCRIPTOR_TABLE_H_
#define GRANIT_BACKEND_VULKAN_BINDLESS_DESCRIPTOR_TABLE_H_

#include <cstdint>

#include <volk.h>

#include <granit/core/result.h>

namespace granit::detail {

class vulkan_device;

/** 内部 Vulkan Descriptor Indexing 表；GPU 安全回收由上层提交完成后负责。 */
class vulkan_bindless_descriptor_table {
public:
  vulkan_bindless_descriptor_table() = default;
  ~vulkan_bindless_descriptor_table() = default;

  vulkan_bindless_descriptor_table(const vulkan_bindless_descriptor_table&) = delete;
  vulkan_bindless_descriptor_table& operator=(const vulkan_bindless_descriptor_table&) = delete;

  [[nodiscard]] granit_result initialize(const vulkan_device& device,
                                         std::uint32_t sampled_texture_capacity,
                                         std::uint32_t sampler_capacity) noexcept;
  void destroy(const vulkan_device& device) noexcept;
  [[nodiscard]] granit_result update_sampled_texture(const vulkan_device& device,
                                                     std::uint32_t index,
                                                     VkImageView image_view) noexcept;
  [[nodiscard]] granit_result update_sampler(const vulkan_device& device, std::uint32_t index,
                                             VkSampler sampler) noexcept;

  [[nodiscard]] bool valid() const noexcept { return descriptor_set_ != VK_NULL_HANDLE; }
  [[nodiscard]] VkDescriptorSetLayout layout() const noexcept { return layout_; }
  [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept { return descriptor_set_; }

private:
  VkDescriptorSetLayout layout_{VK_NULL_HANDLE};
  VkDescriptorPool pool_{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
  std::uint32_t sampled_texture_capacity_{};
  std::uint32_t sampler_capacity_{};
};

} // namespace granit::detail

#endif
