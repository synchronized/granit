// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/renderer_state_utils.h"

#include "core/texture_format.h"

#include <algorithm>
#include <array>

namespace granit::detail {

granit_texture_format map_swapchain_format(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_B8G8R8A8_SRGB:
    return GRANIT_TEXTURE_FORMAT_BGRA8_SRGB;
  case VK_FORMAT_B8G8R8A8_UNORM:
    return GRANIT_TEXTURE_FORMAT_BGRA8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  default:
    return GRANIT_TEXTURE_FORMAT_UNDEFINED;
  }
}

bool same_image_subresource(const vulkan_image_access& left,
                            const vulkan_image_access& right) noexcept {
  return left.image == right.image && left.range.aspectMask == right.range.aspectMask &&
         left.range.baseMipLevel == right.range.baseMipLevel &&
         left.range.levelCount == right.range.levelCount &&
         left.range.baseArrayLayer == right.range.baseArrayLayer &&
         left.range.layerCount == right.range.layerCount;
}

void store_unit_image_accesses(std::vector<vulkan_image_access>& states,
                               const vulkan_image_access& access) {
  constexpr std::array aspects{VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                               VK_IMAGE_ASPECT_STENCIL_BIT};
  for (const auto aspect : aspects) {
    if ((access.range.aspectMask & aspect) == 0)
      continue;
    for (std::uint32_t mip_offset = 0; mip_offset < access.range.levelCount; ++mip_offset) {
      for (std::uint32_t layer_offset = 0; layer_offset < access.range.layerCount; ++layer_offset) {
        auto unit = access;
        unit.range = {static_cast<VkImageAspectFlags>(aspect),
                      access.range.baseMipLevel + mip_offset, 1,
                      access.range.baseArrayLayer + layer_offset, 1};
        const auto found = find_image_subresource(states, unit);
        if (found == states.end())
          states.push_back(unit);
        else
          *found = unit;
      }
    }
  }
}

VkBufferUsageFlags map_buffer_usage(granit_buffer_usage usage) noexcept {
  VkBufferUsageFlags flags{};
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_VERTEX_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_INDEX_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_UNIFORM_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_STORAGE_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if ((usage & GRANIT_BUFFER_USAGE_INDIRECT_BIT) != 0) {
    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }
  return flags;
}

vulkan_memory_location map_memory_location(granit_memory_location location) noexcept {
  switch (location) {
  case GRANIT_MEMORY_LOCATION_DEVICE:
    return vulkan_memory_location::device;
  case GRANIT_MEMORY_LOCATION_UPLOAD:
    return vulkan_memory_location::upload;
  case GRANIT_MEMORY_LOCATION_READBACK:
    return vulkan_memory_location::readback;
  default:
    return vulkan_memory_location::automatic;
  }
}

VkFormat map_texture_format(granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RG8_UNORM:
    return VK_FORMAT_R8G8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case GRANIT_TEXTURE_FORMAT_RGBA8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case GRANIT_TEXTURE_FORMAT_BGRA8_UNORM:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case GRANIT_TEXTURE_FORMAT_BGRA8_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case GRANIT_TEXTURE_FORMAT_D16_UNORM:
    return VK_FORMAT_D16_UNORM;
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
    return VK_FORMAT_D32_SFLOAT;
  case GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT:
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  case GRANIT_TEXTURE_FORMAT_BC1_RGBA_UNORM:
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
  case GRANIT_TEXTURE_FORMAT_BC1_RGBA_SRGB:
    return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
  case GRANIT_TEXTURE_FORMAT_BC3_RGBA_UNORM:
    return VK_FORMAT_BC3_UNORM_BLOCK;
  case GRANIT_TEXTURE_FORMAT_BC3_RGBA_SRGB:
    return VK_FORMAT_BC3_SRGB_BLOCK;
  case GRANIT_TEXTURE_FORMAT_BC5_RG_UNORM:
    return VK_FORMAT_BC5_UNORM_BLOCK;
  case GRANIT_TEXTURE_FORMAT_BC7_RGBA_UNORM:
    return VK_FORMAT_BC7_UNORM_BLOCK;
  case GRANIT_TEXTURE_FORMAT_BC7_RGBA_SRGB:
    return VK_FORMAT_BC7_SRGB_BLOCK;
  case GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_UNORM:
    return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
  case GRANIT_TEXTURE_FORMAT_ETC2_RGBA8_SRGB:
    return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
  case GRANIT_TEXTURE_FORMAT_ASTC_4X4_UNORM:
    return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
  case GRANIT_TEXTURE_FORMAT_ASTC_4X4_SRGB:
    return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

VkFormat map_vertex_format(granit_vertex_format format) noexcept {
  switch (format) {
  case GRANIT_VERTEX_FORMAT_FLOAT32:
    return VK_FORMAT_R32_SFLOAT;
  case GRANIT_VERTEX_FORMAT_FLOAT32X2:
    return VK_FORMAT_R32G32_SFLOAT;
  case GRANIT_VERTEX_FORMAT_FLOAT32X3:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case GRANIT_VERTEX_FORMAT_FLOAT32X4:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case GRANIT_VERTEX_FORMAT_UINT32:
    return VK_FORMAT_R32_UINT;
  case GRANIT_VERTEX_FORMAT_UINT32X2:
    return VK_FORMAT_R32G32_UINT;
  case GRANIT_VERTEX_FORMAT_UINT32X3:
    return VK_FORMAT_R32G32B32_UINT;
  case GRANIT_VERTEX_FORMAT_UINT32X4:
    return VK_FORMAT_R32G32B32A32_UINT;
  case GRANIT_VERTEX_FORMAT_SINT32:
    return VK_FORMAT_R32_SINT;
  case GRANIT_VERTEX_FORMAT_SINT32X2:
    return VK_FORMAT_R32G32_SINT;
  case GRANIT_VERTEX_FORMAT_SINT32X3:
    return VK_FORMAT_R32G32B32_SINT;
  case GRANIT_VERTEX_FORMAT_SINT32X4:
    return VK_FORMAT_R32G32B32A32_SINT;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

VkImageAspectFlags default_aspect(granit_texture_format format) noexcept {
  if (format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
      format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT) {
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  return depth_stencil_texture_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT
                                              : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageAspectFlags map_texture_aspect(granit_texture_aspect aspect) noexcept {
  VkImageAspectFlags flags{};
  if ((aspect & GRANIT_TEXTURE_ASPECT_COLOR_BIT) != 0)
    flags |= VK_IMAGE_ASPECT_COLOR_BIT;
  if ((aspect & GRANIT_TEXTURE_ASPECT_DEPTH_BIT) != 0)
    flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
  if ((aspect & GRANIT_TEXTURE_ASPECT_STENCIL_BIT) != 0)
    flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
  return flags;
}

VkAttachmentLoadOp map_attachment_load(granit_attachment_load_operation value) noexcept {
  return value == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD
             ? VK_ATTACHMENT_LOAD_OP_LOAD
             : (value == GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                                : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
}

VkAttachmentStoreOp map_attachment_store(granit_attachment_store_operation value) noexcept {
  return value == GRANIT_ATTACHMENT_STORE_OPERATION_STORE ? VK_ATTACHMENT_STORE_OP_STORE
                                                          : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkComponentSwizzle map_component_swizzle(granit_component_swizzle swizzle) noexcept {
  constexpr std::array values{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_ZERO,
                              VK_COMPONENT_SWIZZLE_ONE,      VK_COMPONENT_SWIZZLE_R,
                              VK_COMPONENT_SWIZZLE_G,        VK_COMPONENT_SWIZZLE_B,
                              VK_COMPONENT_SWIZZLE_A};
  return values[swizzle];
}

VkImageUsageFlags map_texture_usage(granit_texture_usage usage) noexcept {
  VkImageUsageFlags flags{};
  if ((usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) != 0)
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) != 0)
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_SAMPLED_BIT) != 0)
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_STORAGE_BIT) != 0)
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if ((usage & GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
    flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  return flags;
}

} // namespace granit::detail
