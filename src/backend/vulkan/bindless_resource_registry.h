// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_BINDLESS_RESOURCE_REGISTRY_H_
#define GRANIT_BACKEND_VULKAN_BINDLESS_RESOURCE_REGISTRY_H_

#include "backend/vulkan/bindless_descriptor_table.h"

#include <cstdint>

#include <granit/core/result.h>

#include "renderer/resource_table.hpp"

namespace granit::detail {

class vulkan_device;

/** 将 CPU generation 索引与 Vulkan Descriptor Table 绑定的内部实验 registry。 */
class vulkan_bindless_resource_registry {
public:
  explicit vulkan_bindless_resource_registry(std::uint64_t owner)
      : texture_table_(0), sampler_table_(0), owner_(owner) {}
  ~vulkan_bindless_resource_registry() = default;

  vulkan_bindless_resource_registry(const vulkan_bindless_resource_registry&) = delete;
  vulkan_bindless_resource_registry& operator=(const vulkan_bindless_resource_registry&) = delete;

  [[nodiscard]] granit_result initialize(const vulkan_device& device,
                                         std::uint32_t sampled_texture_capacity,
                                         std::uint32_t sampler_capacity) noexcept;
  void destroy(const vulkan_device& device) noexcept;

  [[nodiscard]] granit_result register_sampled_texture(const vulkan_device& device,
                                                       VkImageView image_view,
                                                       std::uint64_t resource,
                                                       std::uint64_t& handle) noexcept;
  [[nodiscard]] granit_result register_sampler(const vulkan_device& device, VkSampler sampler,
                                               std::uint64_t resource,
                                               std::uint64_t& handle) noexcept;
  [[nodiscard]] granit_result release(std::uint64_t handle, internal::resource_table_type type,
                                      std::uint64_t retire_after) noexcept;
  void collect(std::uint64_t completed) noexcept {
    texture_table_.collect(completed);
    sampler_table_.collect(completed);
  }

  [[nodiscard]] std::uint32_t sampled_texture_capacity() const noexcept {
    return texture_table_.capacity();
  }
  [[nodiscard]] std::uint32_t sampler_capacity() const noexcept {
    return sampler_table_.capacity();
  }
  [[nodiscard]] VkDescriptorSetLayout layout() const noexcept { return descriptors_.layout(); }
  [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept {
    return descriptors_.descriptor_set();
  }

private:
  [[nodiscard]] granit_result register_resource(const vulkan_device& device, VkImageView image_view,
                                                VkSampler sampler, std::uint64_t resource,
                                                internal::resource_table_type type,
                                                std::uint64_t& handle) noexcept;

  internal::resource_table texture_table_;
  internal::resource_table sampler_table_;
  std::uint64_t owner_{};
  vulkan_bindless_descriptor_table descriptors_;
};

} // namespace granit::detail

#endif
