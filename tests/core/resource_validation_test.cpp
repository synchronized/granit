// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/resource_validation.h"

#include <catch2/catch_all.hpp>

#include <limits>

using granit::detail::validate_buffer_desc;
using granit::detail::validate_color_attachment_desc;
using granit::detail::validate_depth_stencil_attachment_desc;
using granit::detail::validate_rendering_desc;
using granit::detail::validate_sampler_desc;
using granit::detail::validate_texture_desc;
using granit::detail::validate_texture_view_desc;

TEST_CASE("资源描述遵守 struct_size 扩展规则", "[resource][abi]") {
  granit_buffer_desc buffer = GRANIT_BUFFER_DESC_INIT;
  buffer.usage = GRANIT_BUFFER_USAGE_VERTEX_BIT;
  buffer.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
  buffer.size = 256;
  buffer.struct_size = GRANIT_BUFFER_DESC_VERSION_1_SIZE - 1;
  CHECK(validate_buffer_desc(buffer) == GRANIT_ERROR_INVALID_ARGUMENT);
  buffer.struct_size = GRANIT_BUFFER_DESC_VERSION_1_SIZE + 64;
  CHECK(validate_buffer_desc(buffer) == GRANIT_SUCCESS);

  granit_texture_desc texture = GRANIT_TEXTURE_DESC_INIT;
  texture.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  texture.struct_size = GRANIT_TEXTURE_DESC_VERSION_1_SIZE - 1;
  CHECK(validate_texture_desc(texture) == GRANIT_ERROR_INVALID_ARGUMENT);
  texture.struct_size = GRANIT_TEXTURE_DESC_VERSION_1_SIZE + 64;
  CHECK(validate_texture_desc(texture) == GRANIT_SUCCESS);

  granit_texture_view_desc view = GRANIT_TEXTURE_VIEW_DESC_INIT;
  view.struct_size = GRANIT_TEXTURE_VIEW_DESC_VERSION_1_SIZE - 1;
  CHECK(validate_texture_view_desc(view) == GRANIT_ERROR_INVALID_ARGUMENT);
  view.struct_size = GRANIT_TEXTURE_VIEW_DESC_VERSION_1_SIZE + 64;
  CHECK(validate_texture_view_desc(view) == GRANIT_SUCCESS);

  granit_sampler_desc sampler = GRANIT_SAMPLER_DESC_INIT;
  sampler.struct_size = GRANIT_SAMPLER_DESC_VERSION_1_SIZE - 1;
  CHECK(validate_sampler_desc(sampler) == GRANIT_ERROR_INVALID_ARGUMENT);
  sampler.struct_size = GRANIT_SAMPLER_DESC_VERSION_1_SIZE + 64;
  CHECK(validate_sampler_desc(sampler) == GRANIT_SUCCESS);

  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = UINT64_C(1);
  color.struct_size = GRANIT_COLOR_ATTACHMENT_DESC_VERSION_1_SIZE - 1;
  CHECK(validate_color_attachment_desc(color) == GRANIT_ERROR_INVALID_ARGUMENT);
  color.struct_size = GRANIT_COLOR_ATTACHMENT_DESC_VERSION_1_SIZE + 64;
  CHECK(validate_color_attachment_desc(color) == GRANIT_SUCCESS);

  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.area.width = 1;
  rendering.area.height = 1;
  rendering.struct_size = GRANIT_RENDERING_DESC_VERSION_1_SIZE - 1;
  CHECK(validate_rendering_desc(rendering) == GRANIT_ERROR_INVALID_ARGUMENT);
  rendering.struct_size = GRANIT_RENDERING_DESC_VERSION_1_SIZE + 64;
  CHECK(validate_rendering_desc(rendering) == GRANIT_SUCCESS);
}

TEST_CASE("Buffer 描述拒绝空大小和未知用途", "[resource][validation]") {
  granit_buffer_desc desc{
      .struct_size = GRANIT_BUFFER_DESC_VERSION_1_SIZE,
      .usage = GRANIT_BUFFER_USAGE_VERTEX_BIT,
      .memory_location = GRANIT_MEMORY_LOCATION_DEVICE,
      .reserved = 0,
      .size = 256,
      .reserved_2 = 0,
  };
  CHECK(validate_buffer_desc(desc) == GRANIT_SUCCESS);
  desc.size = 0;
  CHECK(validate_buffer_desc(desc) == GRANIT_ERROR_INVALID_ARGUMENT);
  desc.size = 256;
  desc.usage |= UINT32_C(1) << 31;
  CHECK(validate_buffer_desc(desc) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Texture 描述区分非法组合和未实现能力", "[resource][validation]") {
  granit_texture_desc desc{
      .struct_size = GRANIT_TEXTURE_DESC_VERSION_1_SIZE,
      .dimension = GRANIT_TEXTURE_DIMENSION_2D,
      .format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
      .usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT,
      .memory_location = GRANIT_MEMORY_LOCATION_DEVICE,
      .width = 64,
      .height = 64,
      .depth = 1,
      .mip_levels = 1,
      .array_layers = 1,
      .sample_count = GRANIT_SAMPLE_COUNT_1,
      .reserved = 0,
  };
  CHECK(validate_texture_desc(desc) == GRANIT_SUCCESS);
  desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  desc.mip_levels = 1;
  desc.sample_count = GRANIT_SAMPLE_COUNT_4;
  CHECK(validate_texture_desc(desc) == GRANIT_SUCCESS);
  desc.mip_levels = 2;
  CHECK(validate_texture_desc(desc) == GRANIT_ERROR_UNSUPPORTED);
  desc.sample_count = GRANIT_SAMPLE_COUNT_1;
  desc.usage = GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  CHECK(validate_texture_desc(desc) == GRANIT_ERROR_INVALID_ARGUMENT);
  desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  desc.mip_levels = 2;
  CHECK(validate_texture_desc(desc) == GRANIT_SUCCESS);
  desc.mip_levels = 7;
  CHECK(validate_texture_desc(desc) == GRANIT_SUCCESS);
  desc.mip_levels = 8;
  CHECK(validate_texture_desc(desc) == GRANIT_ERROR_UNSUPPORTED);

  desc.dimension = GRANIT_TEXTURE_DIMENSION_CUBE;
  desc.mip_levels = 7;
  desc.array_layers = 6;
  CHECK(validate_texture_desc(desc) == GRANIT_SUCCESS);
  desc.height = 32;
  CHECK(validate_texture_desc(desc) == GRANIT_ERROR_UNSUPPORTED);
}

TEST_CASE("View 和 Sampler 验证首期支持范围", "[resource][validation]") {
  const granit_texture_view_desc view{
      .struct_size = GRANIT_TEXTURE_VIEW_DESC_VERSION_1_SIZE,
      .dimension = GRANIT_TEXTURE_DIMENSION_2D,
      .format = GRANIT_TEXTURE_FORMAT_UNDEFINED,
      .reserved = 0,
      .range = {.aspect = GRANIT_TEXTURE_ASPECT_AUTOMATIC,
                .base_mip_level = 0,
                .mip_level_count = 1,
                .base_array_layer = 0,
                .array_layer_count = 1},
      .components = {.red = GRANIT_COMPONENT_SWIZZLE_IDENTITY,
                     .green = GRANIT_COMPONENT_SWIZZLE_IDENTITY,
                     .blue = GRANIT_COMPONENT_SWIZZLE_IDENTITY,
                     .alpha = GRANIT_COMPONENT_SWIZZLE_IDENTITY},
  };
  CHECK(validate_texture_view_desc(view) == GRANIT_SUCCESS);
  auto cube_view = view;
  cube_view.dimension = GRANIT_TEXTURE_DIMENSION_CUBE;
  cube_view.range.mip_level_count = 4;
  cube_view.range.array_layer_count = 6;
  CHECK(validate_texture_view_desc(cube_view) == GRANIT_SUCCESS);
  cube_view.components.alpha = UINT32_C(99);
  CHECK(validate_texture_view_desc(cube_view) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit_sampler_desc sampler{
      .struct_size = GRANIT_SAMPLER_DESC_VERSION_1_SIZE,
      .mag_filter = GRANIT_FILTER_LINEAR,
      .min_filter = GRANIT_FILTER_LINEAR,
      .mipmap_filter = GRANIT_MIPMAP_FILTER_NEAREST,
      .address_mode_u = GRANIT_ADDRESS_MODE_REPEAT,
      .address_mode_v = GRANIT_ADDRESS_MODE_REPEAT,
      .address_mode_w = GRANIT_ADDRESS_MODE_REPEAT,
      .compare_operation = GRANIT_COMPARE_OPERATION_DISABLED,
      .anisotropy_enabled = 0,
      .max_anisotropy = 1.0F,
      .lod_bias = 0.0F,
      .min_lod = 0.0F,
      .max_lod = 1.0F,
      .reserved = 0,
  };
  CHECK(validate_sampler_desc(sampler) == GRANIT_SUCCESS);
  sampler.anisotropy_enabled = 1;
  CHECK(validate_sampler_desc(sampler) == GRANIT_SUCCESS);
  sampler.max_anisotropy = 0.5F;
  CHECK(validate_sampler_desc(sampler) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Render Target Attachment 拒绝未初始化操作和非法清除值", "[resource][attachment]") {
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = UINT64_C(1);
  CHECK(validate_color_attachment_desc(color) == GRANIT_SUCCESS);
  color.resolve_view = UINT64_C(2);
  CHECK(validate_color_attachment_desc(color) == GRANIT_SUCCESS);
  color.reserved_2 = 1;
  CHECK(validate_color_attachment_desc(color) == GRANIT_ERROR_INVALID_ARGUMENT);
  color.reserved_2 = 0;
  color.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_UNDEFINED;
  CHECK(validate_color_attachment_desc(color) == GRANIT_ERROR_INVALID_ARGUMENT);
  color.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD;
  color.clear_value.red = std::numeric_limits<float>::infinity();
  CHECK(validate_color_attachment_desc(color) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth.view = UINT64_C(2);
  CHECK(validate_depth_stencil_attachment_desc(depth) == GRANIT_SUCCESS);
  depth.clear_value.depth = -0.01F;
  CHECK(validate_depth_stencil_attachment_desc(depth) == GRANIT_ERROR_INVALID_ARGUMENT);
  depth.clear_value.depth = 1.01F;
  CHECK(validate_depth_stencil_attachment_desc(depth) == GRANIT_ERROR_INVALID_ARGUMENT);
  depth.clear_value.depth = 0.5F;
  depth.stencil_store_operation = UINT32_C(99);
  CHECK(validate_depth_stencil_attachment_desc(depth) == GRANIT_ERROR_INVALID_ARGUMENT);
}
