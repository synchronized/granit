// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/bindless_resource_registry.h"

#include "backend/vulkan/device.h"

namespace granit::detail {

granit_result
vulkan_bindless_resource_registry::initialize(const vulkan_device& device,
                                              std::uint32_t sampled_texture_capacity,
                                              std::uint32_t sampler_capacity) noexcept {
  if (owner_ == 0 || descriptors_.valid())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (sampled_texture_capacity == UINT32_MAX || sampler_capacity == UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  texture_table_ = internal::resource_table{sampled_texture_capacity};
  sampler_table_ = internal::resource_table{sampler_capacity};
  // Resource Table 的 0 号索引保留为无效值，Descriptor 数组需要额外保留该槽位。
  return descriptors_.initialize(device, sampled_texture_capacity + 1, sampler_capacity + 1);
}

void vulkan_bindless_resource_registry::destroy(const vulkan_device& device) noexcept {
  descriptors_.destroy(device);
}

granit_result vulkan_bindless_resource_registry::register_sampled_texture(
    const vulkan_device& device, VkImageView image_view, std::uint64_t resource,
    std::uint64_t& handle) noexcept {
  return register_resource(device, image_view, VK_NULL_HANDLE, resource,
                           internal::resource_table_type::texture_view, handle);
}

granit_result vulkan_bindless_resource_registry::register_sampler(const vulkan_device& device,
                                                                  VkSampler sampler,
                                                                  std::uint64_t resource,
                                                                  std::uint64_t& handle) noexcept {
  return register_resource(device, VK_NULL_HANDLE, sampler, resource,
                           internal::resource_table_type::sampler, handle);
}

granit_result vulkan_bindless_resource_registry::release(std::uint64_t handle,
                                                         internal::resource_table_type type,
                                                         std::uint64_t retire_after) noexcept {
  auto& table =
      type == internal::resource_table_type::texture_view ? texture_table_ : sampler_table_;
  internal::resource_table_entry entry;
  const auto resolve_result = table.resolve({handle}, owner_, type, entry);
  if (resolve_result == internal::resource_table_result::type_mismatch)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (resolve_result != internal::resource_table_result::success)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto result = table.release({handle}, retire_after);
  if (result == internal::resource_table_result::success)
    return GRANIT_SUCCESS;
  if (result == internal::resource_table_result::invalid_handle)
    return GRANIT_ERROR_INVALID_HANDLE;
  return GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result vulkan_bindless_resource_registry::register_resource(
    const vulkan_device& device, VkImageView image_view, VkSampler sampler, std::uint64_t resource,
    internal::resource_table_type type, std::uint64_t& handle) noexcept {
  handle = 0;
  if (!descriptors_.valid() || resource == 0 ||
      (type == internal::resource_table_type::texture_view && image_view == VK_NULL_HANDLE) ||
      (type == internal::resource_table_type::sampler && sampler == VK_NULL_HANDLE))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& table =
      type == internal::resource_table_type::texture_view ? texture_table_ : sampler_table_;
  const auto entry = table.allocate(owner_, type, resource);
  if (!entry)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto index = static_cast<std::uint32_t>(entry->value);
  const auto descriptor_result =
      type == internal::resource_table_type::texture_view
          ? descriptors_.update_sampled_texture(device, index, image_view)
          : descriptors_.update_sampler(device, index, sampler);
  if (descriptor_result != GRANIT_SUCCESS) {
    static_cast<void>(table.release(*entry, 0));
    table.collect(0);
    return descriptor_result;
  }
  handle = entry->value;
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
