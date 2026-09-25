// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <stddef.h>
#include <stdint.h>

#include <granit/granit.h>
#include <granit/renderer/native_surface.h>

#include "snapshots/0.36.0/core_layout.h"

#define GRANIT_ABI_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

#if UINTPTR_MAX == UINT64_MAX
GRANIT_ABI_ASSERT(granit_036_core_renderer_desc_size,
                  sizeof(granit_renderer_desc) == GRANIT_ABI_036_CORE_RENDERER_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_036_core_renderer_desc_diagnostic_callback,
                  offsetof(granit_renderer_desc, diagnostic_callback) ==
                      GRANIT_ABI_036_CORE_RENDERER_DESC_DIAGNOSTIC_CALLBACK);
GRANIT_ABI_ASSERT(granit_036_core_renderer_desc_backend,
                  offsetof(granit_renderer_desc, backend) ==
                      GRANIT_ABI_036_CORE_RENDERER_DESC_BACKEND);
GRANIT_ABI_ASSERT(granit_036_core_buffer_desc_size,
                  sizeof(granit_buffer_desc) == GRANIT_ABI_036_CORE_BUFFER_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_036_core_buffer_desc_byte_size,
                  offsetof(granit_buffer_desc, size) == GRANIT_ABI_036_CORE_BUFFER_DESC_BYTE_SIZE);
GRANIT_ABI_ASSERT(granit_036_core_texture_desc_size,
                  sizeof(granit_texture_desc) == GRANIT_ABI_036_CORE_TEXTURE_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_036_core_texture_desc_mip_levels,
                  offsetof(granit_texture_desc, mip_levels) ==
                      GRANIT_ABI_036_CORE_TEXTURE_DESC_MIP_LEVELS);
GRANIT_ABI_ASSERT(granit_036_core_graphics_pipeline_desc_size,
                  sizeof(granit_graphics_pipeline_desc) ==
                      GRANIT_ABI_036_CORE_GRAPHICS_PIPELINE_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_036_core_graphics_pipeline_desc_color_formats,
                  offsetof(granit_graphics_pipeline_desc, color_formats) ==
                      GRANIT_ABI_036_CORE_GRAPHICS_PIPELINE_DESC_COLOR_FORMATS);
GRANIT_ABI_ASSERT(granit_036_core_graphics_pipeline_desc_depth_bias,
                  offsetof(granit_graphics_pipeline_desc, depth_bias) ==
                      GRANIT_ABI_036_CORE_GRAPHICS_PIPELINE_DESC_DEPTH_BIAS);
GRANIT_ABI_ASSERT(granit_036_core_surface_desc_size,
                  sizeof(granit_surface_desc) == GRANIT_ABI_036_CORE_SURFACE_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_036_core_surface_desc_source,
                  offsetof(granit_surface_desc, source) == GRANIT_ABI_036_CORE_SURFACE_DESC_SOURCE);
GRANIT_ABI_ASSERT(granit_036_core_frame_context_desc_size,
                  sizeof(granit_frame_context_desc) == GRANIT_ABI_036_CORE_FRAME_CONTEXT_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_036_core_frame_info_size,
                  sizeof(granit_frame_info) == GRANIT_ABI_036_CORE_FRAME_INFO_SIZE);
#endif

#undef GRANIT_ABI_ASSERT
