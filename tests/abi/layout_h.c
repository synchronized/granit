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

GRANIT_ABI_ASSERT(granit_abi_pipeline_layout_desc_size, sizeof(granit_pipeline_layout_desc) == 24);
GRANIT_ABI_ASSERT(granit_abi_pipeline_layout_desc_layouts,
                  offsetof(granit_pipeline_layout_desc, bind_group_layouts) == 8);
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

typedef struct granit_abi_renderer_desc_alignment_probe {
  char prefix;
  granit_renderer_desc value;
} granit_abi_renderer_desc_alignment_probe;
GRANIT_ABI_ASSERT(granit_abi_renderer_desc_alignment,
                  offsetof(granit_abi_renderer_desc_alignment_probe, value) == 8);
#endif

#undef GRANIT_ABI_ASSERT
