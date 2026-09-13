// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device_utils.h"

namespace granit::detail::webgpu_native {

WGPUTextureFormat to_native_texture_format(webgpu_texture_format format) noexcept {
  switch (format) {
  case GRANIT_WEBGPU_TEXTURE_FORMAT_R8_UNORM:
    return WGPUTextureFormat_R8Unorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RG8_UNORM:
    return WGPUTextureFormat_RG8Unorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_UNORM:
    return WGPUTextureFormat_RGBA8Unorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BGRA8_UNORM:
    return WGPUTextureFormat_BGRA8Unorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_SRGB:
    return WGPUTextureFormat_RGBA8UnormSrgb;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT:
    return WGPUTextureFormat_Depth32Float;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA16_FLOAT:
    return WGPUTextureFormat_RGBA16Float;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC1_RGBA_UNORM:
    return WGPUTextureFormat_BC1RGBAUnorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC1_RGBA_SRGB:
    return WGPUTextureFormat_BC1RGBAUnormSrgb;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC3_RGBA_UNORM:
    return WGPUTextureFormat_BC3RGBAUnorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC3_RGBA_SRGB:
    return WGPUTextureFormat_BC3RGBAUnormSrgb;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC5_RG_UNORM:
    return WGPUTextureFormat_BC5RGUnorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC7_RGBA_UNORM:
    return WGPUTextureFormat_BC7RGBAUnorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC7_RGBA_SRGB:
    return WGPUTextureFormat_BC7RGBAUnormSrgb;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_ETC2_RGBA8_UNORM:
    return WGPUTextureFormat_ETC2RGBA8Unorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_ETC2_RGBA8_SRGB:
    return WGPUTextureFormat_ETC2RGBA8UnormSrgb;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_ASTC_4X4_UNORM:
    return WGPUTextureFormat_ASTC4x4Unorm;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_ASTC_4X4_SRGB:
    return WGPUTextureFormat_ASTC4x4UnormSrgb;
  default:
    return WGPUTextureFormat_Undefined;
  }
}

WGPUBlendFactor to_native_blend_factor(webgpu_blend_factor factor) noexcept {
  switch (factor) {
  case GRANIT_WEBGPU_BLEND_FACTOR_ZERO:
    return WGPUBlendFactor_Zero;
  case GRANIT_WEBGPU_BLEND_FACTOR_ONE:
    return WGPUBlendFactor_One;
  case GRANIT_WEBGPU_BLEND_FACTOR_SOURCE_COLOR:
    return WGPUBlendFactor_Src;
  case GRANIT_WEBGPU_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR:
    return WGPUBlendFactor_OneMinusSrc;
  case GRANIT_WEBGPU_BLEND_FACTOR_SOURCE_ALPHA:
    return WGPUBlendFactor_SrcAlpha;
  case GRANIT_WEBGPU_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA:
    return WGPUBlendFactor_OneMinusSrcAlpha;
  case GRANIT_WEBGPU_BLEND_FACTOR_DESTINATION_COLOR:
    return WGPUBlendFactor_Dst;
  case GRANIT_WEBGPU_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR:
    return WGPUBlendFactor_OneMinusDst;
  case GRANIT_WEBGPU_BLEND_FACTOR_DESTINATION_ALPHA:
    return WGPUBlendFactor_DstAlpha;
  case GRANIT_WEBGPU_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA:
    return WGPUBlendFactor_OneMinusDstAlpha;
  default:
    return WGPUBlendFactor_Undefined;
  }
}

WGPUBlendOperation to_native_blend_operation(webgpu_blend_operation operation) noexcept {
  switch (operation) {
  case GRANIT_WEBGPU_BLEND_OPERATION_ADD:
    return WGPUBlendOperation_Add;
  case GRANIT_WEBGPU_BLEND_OPERATION_SUBTRACT:
    return WGPUBlendOperation_Subtract;
  case GRANIT_WEBGPU_BLEND_OPERATION_REVERSE_SUBTRACT:
    return WGPUBlendOperation_ReverseSubtract;
  case GRANIT_WEBGPU_BLEND_OPERATION_MIN:
    return WGPUBlendOperation_Min;
  case GRANIT_WEBGPU_BLEND_OPERATION_MAX:
    return WGPUBlendOperation_Max;
  default:
    return WGPUBlendOperation_Undefined;
  }
}

WGPUCompareFunction to_native_compare_operation(webgpu_compare_operation operation) noexcept {
  switch (operation) {
  case GRANIT_WEBGPU_COMPARE_OPERATION_NEVER:
    return WGPUCompareFunction_Never;
  case GRANIT_WEBGPU_COMPARE_OPERATION_LESS:
    return WGPUCompareFunction_Less;
  case GRANIT_WEBGPU_COMPARE_OPERATION_EQUAL:
    return WGPUCompareFunction_Equal;
  case GRANIT_WEBGPU_COMPARE_OPERATION_LESS_EQUAL:
    return WGPUCompareFunction_LessEqual;
  case GRANIT_WEBGPU_COMPARE_OPERATION_GREATER:
    return WGPUCompareFunction_Greater;
  case GRANIT_WEBGPU_COMPARE_OPERATION_NOT_EQUAL:
    return WGPUCompareFunction_NotEqual;
  case GRANIT_WEBGPU_COMPARE_OPERATION_GREATER_EQUAL:
    return WGPUCompareFunction_GreaterEqual;
  case GRANIT_WEBGPU_COMPARE_OPERATION_ALWAYS:
    return WGPUCompareFunction_Always;
  default:
    return WGPUCompareFunction_Undefined;
  }
}

texture_block_info texture_block(webgpu_texture_format format) noexcept {
  switch (format) {
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC1_RGBA_UNORM:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC1_RGBA_SRGB:
    return {4, 4, 8};
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC3_RGBA_UNORM:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC3_RGBA_SRGB:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC5_RG_UNORM:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC7_RGBA_UNORM:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BC7_RGBA_SRGB:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_ETC2_RGBA8_UNORM:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_ETC2_RGBA8_SRGB:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_ASTC_4X4_UNORM:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_ASTC_4X4_SRGB:
    return {4, 4, 16};
  case GRANIT_WEBGPU_TEXTURE_FORMAT_R8_UNORM:
    return {1, 1, 1};
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RG8_UNORM:
    return {1, 1, 2};
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_UNORM:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_SRGB:
  case GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT:
    return {1, 1, 4};
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA16_FLOAT:
    return {1, 1, 8};
  default:
    return {};
  }
}

bool valid_texture_block_region(webgpu_texture_format format, std::uint32_t x, std::uint32_t y,
                                std::uint32_t width, std::uint32_t height, std::uint32_t mip_width,
                                std::uint32_t mip_height) noexcept {
  const auto block = texture_block(format);
  return block.bytes != 0 && x % block.width == 0 && y % block.height == 0 &&
         (width % block.width == 0 || x + width == mip_width) &&
         (height % block.height == 0 || y + height == mip_height);
}

WGPUTextureAspect map_texture_aspect(webgpu_texture_aspect aspect) noexcept {
  switch (aspect) {
  case GRANIT_WEBGPU_TEXTURE_ASPECT_ALL:
    return WGPUTextureAspect_All;
  case GRANIT_WEBGPU_TEXTURE_ASPECT_DEPTH:
    return WGPUTextureAspect_DepthOnly;
  case GRANIT_WEBGPU_TEXTURE_ASPECT_STENCIL:
    return WGPUTextureAspect_StencilOnly;
  default:
    return WGPUTextureAspect_Undefined;
  }
}

} // namespace granit::detail::webgpu_native
