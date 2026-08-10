// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_H_
#define GRANIT_PIPELINE_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/export.h>
#include <granit/renderer.h>
#include <granit/resource_types.h>
#include <granit/result.h>
#include <granit/shader.h>
#include <granit/types.h>

/** Pipeline 资源布局句柄。D-03A 只支持空布局。 */
typedef granit_handle granit_pipeline_layout;
/** 图形 Pipeline 句柄。 */
typedef granit_handle granit_graphics_pipeline;

typedef struct granit_pipeline_layout_desc {
  uint32_t struct_size;
  uint32_t reserved;
} granit_pipeline_layout_desc;
#define GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE UINT32_C(8)
#define GRANIT_PIPELINE_LAYOUT_DESC_INIT {GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE, UINT32_C(0)}

/** Dynamic Rendering 图形 Pipeline 描述；输入数组只在调用期间有效。 */
typedef struct granit_graphics_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_pipeline_layout layout;
  granit_shader vertex_shader;
  granit_shader fragment_shader;
  uint32_t color_format_count;
  const granit_texture_format* color_formats;
  granit_texture_format depth_stencil_format;
  granit_sample_count sample_count;
  uint32_t reserved_2;
} granit_graphics_pipeline_desc;
#define GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_1_SIZE                                               \
  ((uint32_t)(offsetof(granit_graphics_pipeline_desc, reserved_2) + sizeof(uint32_t)))
#define GRANIT_GRAPHICS_PIPELINE_DESC_INIT                                                         \
  {(uint32_t)sizeof(granit_graphics_pipeline_desc),                                                \
   UINT32_C(0),                                                                                    \
   GRANIT_NULL_HANDLE,                                                                             \
   GRANIT_NULL_HANDLE,                                                                             \
   GRANIT_NULL_HANDLE,                                                                             \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   GRANIT_SAMPLE_COUNT_1,                                                                          \
   UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_pipeline_layout_create(granit_renderer renderer,
                                                       const granit_pipeline_layout_desc* desc,
                                                       granit_pipeline_layout* layout);
GRANIT_API granit_result granit_pipeline_layout_destroy(granit_renderer renderer,
                                                        granit_pipeline_layout layout);
GRANIT_API granit_result granit_graphics_pipeline_create(granit_renderer renderer,
                                                         const granit_graphics_pipeline_desc* desc,
                                                         granit_graphics_pipeline* pipeline);
GRANIT_API granit_result granit_graphics_pipeline_destroy(granit_renderer renderer,
                                                          granit_graphics_pipeline pipeline);

#ifdef __cplusplus
}
#endif

#endif
