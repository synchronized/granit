// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WEBGPU_DEVICE_UTILS_H_
#define GRANIT_WEBGPU_DEVICE_UTILS_H_

#include "backend/webgpu/types.h"

#include <algorithm>
#include <cstdint>

#include <webgpu/webgpu.h>

namespace granit::detail::webgpu_native {

WGPUTextureFormat to_native_texture_format(webgpu_texture_format format) noexcept;
WGPUBlendFactor to_native_blend_factor(webgpu_blend_factor factor) noexcept;
WGPUBlendOperation to_native_blend_operation(webgpu_blend_operation operation) noexcept;
WGPUCompareFunction to_native_compare_operation(webgpu_compare_operation operation) noexcept;

struct texture_block_info {
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t bytes;
};

texture_block_info texture_block(webgpu_texture_format format) noexcept;
bool valid_texture_block_region(webgpu_texture_format format, std::uint32_t x, std::uint32_t y,
                                std::uint32_t width, std::uint32_t height, std::uint32_t mip_width,
                                std::uint32_t mip_height) noexcept;
WGPUTextureAspect map_texture_aspect(webgpu_texture_aspect aspect) noexcept;

} // namespace granit::detail::webgpu_native

#endif
