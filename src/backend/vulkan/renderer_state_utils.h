// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_RENDERER_STATE_UTILS_H_
#define GRANIT_BACKEND_VULKAN_RENDERER_STATE_UTILS_H_

#include "backend/vulkan/renderer_state.h"

#include <algorithm>
#include <type_traits>

namespace granit::detail {

template <typename Handle> std::uint64_t object_handle_value(Handle handle) noexcept {
  if constexpr (std::is_pointer_v<Handle>)
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
  else
    return static_cast<std::uint64_t>(handle);
}

granit_texture_format map_swapchain_format(VkFormat format) noexcept;
bool same_image_subresource(const vulkan_image_access& left,
                            const vulkan_image_access& right) noexcept;
template <typename States>
auto find_image_subresource(States& states, const vulkan_image_access& access) {
  return std::find_if(states.begin(), states.end(),
                      [&](const auto& state) { return same_image_subresource(state, access); });
}
void store_unit_image_accesses(std::vector<vulkan_image_access>& states,
                               const vulkan_image_access& access);
VkBufferUsageFlags map_buffer_usage(granit_buffer_usage usage) noexcept;
vulkan_memory_location map_memory_location(granit_memory_location location) noexcept;
VkFormat map_texture_format(granit_texture_format format) noexcept;
VkFormat map_vertex_format(granit_vertex_format format) noexcept;
VkImageAspectFlags default_aspect(granit_texture_format format) noexcept;
VkImageAspectFlags map_texture_aspect(granit_texture_aspect aspect) noexcept;
VkAttachmentLoadOp map_attachment_load(granit_attachment_load_operation value) noexcept;
VkAttachmentStoreOp map_attachment_store(granit_attachment_store_operation value) noexcept;
VkComponentSwizzle map_component_swizzle(granit_component_swizzle swizzle) noexcept;
VkImageUsageFlags map_texture_usage(granit_texture_usage usage) noexcept;

} // namespace granit::detail

#endif
