// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/bindless_descriptor_table.h"

#include "backend/vulkan/device.h"
#include "backend/vulkan/result.h"

#include <array>

namespace granit::detail {

namespace {
constexpr std::uint32_t sampled_texture_binding = 0;
constexpr std::uint32_t sampler_binding = 1;
} // namespace

granit_result
vulkan_bindless_descriptor_table::initialize(const vulkan_device& device,
                                             std::uint32_t sampled_texture_capacity,
                                             std::uint32_t sampler_capacity) noexcept {
  if (valid() || !device.valid() || !device.bindless_descriptor_indexing_supported() ||
      sampled_texture_capacity == 0 || sampler_capacity == 0 ||
      sampled_texture_capacity > device.properties().limits.maxDescriptorSetSampledImages ||
      sampler_capacity > device.properties().limits.maxDescriptorSetSamplers) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  const auto partially_bound = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
  const auto variable_count = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
  const std::array<VkDescriptorSetLayoutBinding, 2> bindings{
      VkDescriptorSetLayoutBinding{
          sampled_texture_binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampled_texture_capacity,
          VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
      VkDescriptorSetLayoutBinding{sampler_binding, VK_DESCRIPTOR_TYPE_SAMPLER, sampler_capacity,
                                   VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                                   nullptr}};
  const std::array<VkDescriptorBindingFlags, 2> binding_flags{partially_bound,
                                                              partially_bound | variable_count};
  VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{};
  flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  flags_info.bindingCount = static_cast<std::uint32_t>(binding_flags.size());
  flags_info.pBindingFlags = binding_flags.data();
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.pNext = &flags_info;
  layout_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
  layout_info.pBindings = bindings.data();
  auto result = map_vulkan_result(device.functions().vkCreateDescriptorSetLayout(
      device.native_handle(), &layout_info, nullptr, &layout_));
  if (result != GRANIT_SUCCESS)
    return result;

  const std::array<VkDescriptorPoolSize, 2> pool_sizes{
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampled_texture_capacity},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, sampler_capacity}};
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();
  result = map_vulkan_result(device.functions().vkCreateDescriptorPool(
      device.native_handle(), &pool_info, nullptr, &pool_));
  if (result != GRANIT_SUCCESS) {
    destroy(device);
    return result;
  }

  const std::array<std::uint32_t, 1> variable_counts{sampler_capacity};
  VkDescriptorSetVariableDescriptorCountAllocateInfo variable_info{};
  variable_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
  variable_info.descriptorSetCount = 1;
  variable_info.pDescriptorCounts = variable_counts.data();
  VkDescriptorSetAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.pNext = &variable_info;
  allocate_info.descriptorPool = pool_;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &layout_;
  result = map_vulkan_result(device.functions().vkAllocateDescriptorSets(
      device.native_handle(), &allocate_info, &descriptor_set_));
  if (result != GRANIT_SUCCESS) {
    destroy(device);
    return result;
  }
  sampled_texture_capacity_ = sampled_texture_capacity;
  sampler_capacity_ = sampler_capacity;
  return GRANIT_SUCCESS;
}

void vulkan_bindless_descriptor_table::destroy(const vulkan_device& device) noexcept {
  if (!device.valid())
    return;
  if (pool_ != VK_NULL_HANDLE)
    device.functions().vkDestroyDescriptorPool(device.native_handle(), pool_, nullptr);
  if (layout_ != VK_NULL_HANDLE)
    device.functions().vkDestroyDescriptorSetLayout(device.native_handle(), layout_, nullptr);
  layout_ = VK_NULL_HANDLE;
  pool_ = VK_NULL_HANDLE;
  descriptor_set_ = VK_NULL_HANDLE;
  sampled_texture_capacity_ = 0;
  sampler_capacity_ = 0;
}

granit_result vulkan_bindless_descriptor_table::update_sampled_texture(
    const vulkan_device& device, std::uint32_t index, VkImageView image_view) noexcept {
  if (!valid() || !device.valid() || image_view == VK_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (index >= sampled_texture_capacity_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const VkDescriptorImageInfo image{VK_NULL_HANDLE, image_view,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptor_set_;
  write.dstBinding = sampled_texture_binding;
  write.dstArrayElement = index;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  write.pImageInfo = &image;
  device.functions().vkUpdateDescriptorSets(device.native_handle(), 1, &write, 0, nullptr);
  return GRANIT_SUCCESS;
}

granit_result vulkan_bindless_descriptor_table::update_sampler(const vulkan_device& device,
                                                               std::uint32_t index,
                                                               VkSampler sampler) noexcept {
  if (!valid() || !device.valid() || sampler == VK_NULL_HANDLE || index >= sampler_capacity_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const VkDescriptorImageInfo image{sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptor_set_;
  write.dstBinding = sampler_binding;
  write.dstArrayElement = index;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  write.pImageInfo = &image;
  device.functions().vkUpdateDescriptorSets(device.native_handle(), 1, &write, 0, nullptr);
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
