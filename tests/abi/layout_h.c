// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <stddef.h>
#include <stdint.h>

#include <granit/granit.h>

#define GRANIT_ABI_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

/* 基础类型与常量属于所有平台都必须保持的 C ABI。 */
GRANIT_ABI_ASSERT(granit_abi_handle_size, sizeof(granit_handle) == 8);
GRANIT_ABI_ASSERT(granit_abi_result_size, sizeof(granit_result) == 4);
GRANIT_ABI_ASSERT(granit_abi_null_handle, GRANIT_NULL_HANDLE == UINT64_C(0));
GRANIT_ABI_ASSERT(granit_abi_success, GRANIT_SUCCESS == INT32_C(0));
GRANIT_ABI_ASSERT(granit_abi_first_error, GRANIT_ERROR_UNKNOWN == INT32_C(-1));
GRANIT_ABI_ASSERT(granit_abi_last_error, GRANIT_ERROR_NOT_READY == INT32_C(-14));
GRANIT_ABI_ASSERT(granit_abi_renderer_api_version, GRANIT_RENDERER_API_VERSION_CURRENT == 1);
GRANIT_ABI_ASSERT(granit_abi_validation_bit, GRANIT_RENDERER_ENABLE_VALIDATION_BIT == (1U << 0));
GRANIT_ABI_ASSERT(granit_abi_vertex_usage_bit, GRANIT_BUFFER_USAGE_VERTEX_BIT == (1U << 2));
GRANIT_ABI_ASSERT(granit_abi_color_usage_bit,
                  GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT == (1U << 4));
GRANIT_ABI_ASSERT(granit_abi_rgba8_format, GRANIT_TEXTURE_FORMAT_RGBA8_UNORM == 3);
GRANIT_ABI_ASSERT(granit_abi_d32s8_format, GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT == 11);

/* 当前受支持平台为 64 位；含指针的结构在此建立独立数字基线。 */
#if UINTPTR_MAX == UINT64_MAX
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_size, sizeof(granit_renderer_desc) == 40);
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_application_name,
                  offsetof(granit_renderer_desc, application_name) == 8);
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_flags, offsetof(granit_renderer_desc, flags) == 20);
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_reserved,
                  offsetof(granit_renderer_desc, reserved) == 32);
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_v1, GRANIT_RENDERER_DESC_VERSION_1_SIZE == 24);
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_v2, GRANIT_RENDERER_DESC_VERSION_2_SIZE == 28);
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_v3, GRANIT_RENDERER_DESC_VERSION_3_SIZE == 36);

GRANIT_ABI_ASSERT(granit_abi_buffer_desc_size, sizeof(granit_buffer_desc) == 32);
GRANIT_ABI_ASSERT(granit_abi_buffer_desc_size_field, offsetof(granit_buffer_desc, size) == 16);
GRANIT_ABI_ASSERT(granit_abi_texture_desc_size, sizeof(granit_texture_desc) == 48);
GRANIT_ABI_ASSERT(granit_abi_texture_desc_format, offsetof(granit_texture_desc, format) == 8);
GRANIT_ABI_ASSERT(granit_abi_texture_desc_mips, offsetof(granit_texture_desc, mip_levels) == 32);
GRANIT_ABI_ASSERT(granit_abi_texture_view_desc_size, sizeof(granit_texture_view_desc) == 52);
GRANIT_ABI_ASSERT(granit_abi_texture_view_desc_range,
                  offsetof(granit_texture_view_desc, range) == 16);
GRANIT_ABI_ASSERT(granit_abi_sampler_desc_size, sizeof(granit_sampler_desc) == 56);
GRANIT_ABI_ASSERT(granit_abi_sampler_desc_anisotropy,
                  offsetof(granit_sampler_desc, max_anisotropy) == 36);
GRANIT_ABI_ASSERT(granit_abi_texture_data_layout_size, sizeof(granit_texture_data_layout) == 16);
GRANIT_ABI_ASSERT(granit_abi_texture_write_region_size, sizeof(granit_texture_write_region) == 40);
GRANIT_ABI_ASSERT(granit_abi_subresource_range_size, sizeof(granit_subresource_range) == 20);
GRANIT_ABI_ASSERT(granit_abi_component_mapping_size, sizeof(granit_component_mapping) == 16);

GRANIT_ABI_ASSERT(granit_abi_bind_group_layout_entry_size,
                  sizeof(granit_bind_group_layout_entry) == 16);
GRANIT_ABI_ASSERT(granit_abi_bind_group_layout_desc_size,
                  sizeof(granit_bind_group_layout_desc) == 24);
GRANIT_ABI_ASSERT(granit_abi_bind_group_layout_desc_entries,
                  offsetof(granit_bind_group_layout_desc, entries) == 8);
GRANIT_ABI_ASSERT(granit_abi_bind_group_entry_size, sizeof(granit_bind_group_entry) == 32);
GRANIT_ABI_ASSERT(granit_abi_bind_group_entry_resource,
                  offsetof(granit_bind_group_entry, resource) == 8);
GRANIT_ABI_ASSERT(granit_abi_bind_group_desc_size, sizeof(granit_bind_group_desc) == 32);
GRANIT_ABI_ASSERT(granit_abi_bind_group_desc_entries,
                  offsetof(granit_bind_group_desc, entries) == 16);
GRANIT_ABI_ASSERT(granit_abi_pipeline_layout_desc_size, sizeof(granit_pipeline_layout_desc) == 24);
GRANIT_ABI_ASSERT(granit_abi_pipeline_layout_desc_layouts,
                  offsetof(granit_pipeline_layout_desc, bind_group_layouts) == 8);
GRANIT_ABI_ASSERT(granit_abi_vertex_attribute_size, sizeof(granit_vertex_attribute) == 16);
GRANIT_ABI_ASSERT(granit_abi_vertex_buffer_layout_size, sizeof(granit_vertex_buffer_layout) == 24);
GRANIT_ABI_ASSERT(granit_abi_vertex_buffer_layout_attributes,
                  offsetof(granit_vertex_buffer_layout, attributes) == 16);
GRANIT_ABI_ASSERT(granit_abi_primitive_state_size, sizeof(granit_primitive_state) == 16);
GRANIT_ABI_ASSERT(granit_abi_depth_state_size, sizeof(granit_depth_state) == 16);
GRANIT_ABI_ASSERT(granit_abi_depth_bias_state_size, sizeof(granit_depth_bias_state) == 16);
GRANIT_ABI_ASSERT(granit_abi_color_blend_state_size, sizeof(granit_color_blend_state) == 32);
GRANIT_ABI_ASSERT(granit_abi_compute_pipeline_desc_size,
                  sizeof(granit_compute_pipeline_desc) == 32);
GRANIT_ABI_ASSERT(granit_abi_compute_pipeline_desc_shader,
                  offsetof(granit_compute_pipeline_desc, compute_shader) == 16);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_size,
                  sizeof(granit_graphics_pipeline_desc) == 128);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_formats,
                  offsetof(granit_graphics_pipeline_desc, color_formats) == 40);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_vertex_layouts,
                  offsetof(granit_graphics_pipeline_desc, vertex_buffer_layouts) == 72);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_primitive,
                  offsetof(granit_graphics_pipeline_desc, primitive) == 80);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_blends,
                  offsetof(granit_graphics_pipeline_desc, color_blends) == 112);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_depth_bias,
                  offsetof(granit_graphics_pipeline_desc, depth_bias) == 120);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_v1,
                  GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_1_SIZE == 60);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_v2,
                  GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_2_SIZE == 80);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_v3,
                  GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_3_SIZE == 96);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_v4,
                  GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_4_SIZE == 120);
GRANIT_ABI_ASSERT(granit_abi_graphics_pipeline_desc_v5,
                  GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE == 128);

GRANIT_ABI_ASSERT(granit_abi_buffer_copy_region_size, sizeof(granit_buffer_copy_region) == 24);
GRANIT_ABI_ASSERT(granit_abi_texture_copy_region_size, sizeof(granit_texture_copy_region) == 64);
GRANIT_ABI_ASSERT(granit_abi_texture_copy_region_extent,
                  offsetof(granit_texture_copy_region, width) == 48);
GRANIT_ABI_ASSERT(granit_abi_texture_mipmap_range_size, sizeof(granit_texture_mipmap_range) == 16);
GRANIT_ABI_ASSERT(granit_abi_viewport_size, sizeof(granit_viewport) == 24);
GRANIT_ABI_ASSERT(granit_abi_scissor_size, sizeof(granit_scissor) == 16);
GRANIT_ABI_ASSERT(granit_abi_vertex_buffer_binding_size,
                  sizeof(granit_vertex_buffer_binding) == 16);
GRANIT_ABI_ASSERT(granit_abi_command_recorder_desc_size,
                  sizeof(granit_command_recorder_desc) == 16);
GRANIT_ABI_ASSERT(granit_abi_command_recorder_desc_reserved,
                  offsetof(granit_command_recorder_desc, reserved) == 8);

GRANIT_ABI_ASSERT(granit_abi_clear_color_size, sizeof(granit_clear_color_value) == 16);
GRANIT_ABI_ASSERT(granit_abi_clear_depth_stencil_size,
                  sizeof(granit_clear_depth_stencil_value) == 8);
GRANIT_ABI_ASSERT(granit_abi_color_attachment_desc_size,
                  sizeof(granit_color_attachment_desc) == 48);
GRANIT_ABI_ASSERT(granit_abi_color_attachment_desc_view,
                  offsetof(granit_color_attachment_desc, view) == 16);
GRANIT_ABI_ASSERT(granit_abi_color_attachment_desc_clear,
                  offsetof(granit_color_attachment_desc, clear_value) == 24);
GRANIT_ABI_ASSERT(granit_abi_depth_attachment_desc_size,
                  sizeof(granit_depth_stencil_attachment_desc) == 48);
GRANIT_ABI_ASSERT(granit_abi_depth_attachment_desc_view,
                  offsetof(granit_depth_stencil_attachment_desc, view) == 24);
GRANIT_ABI_ASSERT(granit_abi_rendering_area_size, sizeof(granit_rendering_area) == 16);
GRANIT_ABI_ASSERT(granit_abi_rendering_desc_size, sizeof(granit_rendering_desc) == 56);
GRANIT_ABI_ASSERT(granit_abi_rendering_desc_colors,
                  offsetof(granit_rendering_desc, color_attachments) == 8);
GRANIT_ABI_ASSERT(granit_abi_rendering_desc_depth,
                  offsetof(granit_rendering_desc, depth_stencil_attachment) == 16);
GRANIT_ABI_ASSERT(granit_abi_rendering_desc_area, offsetof(granit_rendering_desc, area) == 24);

GRANIT_ABI_ASSERT(granit_abi_win32_surface_desc_size, sizeof(granit_win32_surface_desc) == 24);
GRANIT_ABI_ASSERT(granit_abi_win32_surface_desc_instance,
                  offsetof(granit_win32_surface_desc, instance) == 8);
GRANIT_ABI_ASSERT(granit_abi_win32_surface_desc_window,
                  offsetof(granit_win32_surface_desc, window) == 16);
GRANIT_ABI_ASSERT(granit_abi_win32_surface_desc_v1, GRANIT_WIN32_SURFACE_DESC_VERSION_1_SIZE == 24);
GRANIT_ABI_ASSERT(granit_abi_swapchain_desc_size, sizeof(granit_swapchain_desc) == 20);
GRANIT_ABI_ASSERT(granit_abi_swapchain_desc_present_mode,
                  offsetof(granit_swapchain_desc, present_mode) == 16);
GRANIT_ABI_ASSERT(granit_abi_swapchain_info_size, sizeof(granit_swapchain_info) == 24);
GRANIT_ABI_ASSERT(granit_abi_swapchain_info_format, offsetof(granit_swapchain_info, format) == 20);
GRANIT_ABI_ASSERT(granit_abi_swapchain_info_v1, GRANIT_SWAPCHAIN_INFO_VERSION_1_SIZE == 20);
GRANIT_ABI_ASSERT(granit_abi_swapchain_info_v2, GRANIT_SWAPCHAIN_INFO_VERSION_2_SIZE == 24);

GRANIT_ABI_ASSERT(granit_abi_shader_desc_size, sizeof(granit_shader_desc) == 40);
GRANIT_ABI_ASSERT(granit_abi_shader_desc_code, offsetof(granit_shader_desc, code) == 8);
GRANIT_ABI_ASSERT(granit_abi_shader_desc_entry, offsetof(granit_shader_desc, entry_point) == 24);
GRANIT_ABI_ASSERT(granit_abi_shader_desc_v1, GRANIT_SHADER_DESC_VERSION_1_SIZE == 40);
GRANIT_ABI_ASSERT(granit_abi_buffer_initial_data_size, sizeof(granit_buffer_initial_data) == 24);
GRANIT_ABI_ASSERT(granit_abi_buffer_initial_data_data,
                  offsetof(granit_buffer_initial_data, data) == 8);
GRANIT_ABI_ASSERT(granit_abi_texture_footprint_size, sizeof(granit_texture_format_footprint) == 32);
GRANIT_ABI_ASSERT(granit_abi_texture_readback_info_size,
                  sizeof(granit_texture_readback_info) == 48);
GRANIT_ABI_ASSERT(granit_abi_texture_readback_required_size,
                  offsetof(granit_texture_readback_info, required_size) == 32);
GRANIT_ABI_ASSERT(granit_abi_timestamp_query_desc_size,
                  sizeof(granit_timestamp_query_pool_desc) == 16);
GRANIT_ABI_ASSERT(granit_abi_upload_batch_desc_size, sizeof(granit_upload_batch_desc) == 16);

typedef struct granit_abi_renderer_desc_alignment_probe {
  char prefix;
  granit_renderer_desc value;
} granit_abi_renderer_desc_alignment_probe;
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_alignment,
                  offsetof(granit_abi_renderer_desc_alignment_probe, value) == 8);
#endif

#undef GRANIT_ABI_ASSERT
