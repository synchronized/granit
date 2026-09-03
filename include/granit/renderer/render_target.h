// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_TARGET_H_
#define GRANIT_RENDER_TARGET_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/renderer/texture.h>

/** 渲染开始时对附件原有内容的处理方式。 */
typedef uint32_t granit_attachment_load_operation;
#define GRANIT_ATTACHMENT_LOAD_OPERATION_UNDEFINED UINT32_C(0)
#define GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD UINT32_C(1)
#define GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR UINT32_C(2)
#define GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD UINT32_C(3)

/** 渲染结束时对附件内容的处理方式。 */
typedef uint32_t granit_attachment_store_operation;
#define GRANIT_ATTACHMENT_STORE_OPERATION_UNDEFINED UINT32_C(0)
#define GRANIT_ATTACHMENT_STORE_OPERATION_STORE UINT32_C(1)
#define GRANIT_ATTACHMENT_STORE_OPERATION_DISCARD UINT32_C(2)

/** 第一版支持的最大颜色附件数量。 */
#define GRANIT_MAX_COLOR_ATTACHMENTS UINT32_C(8)

/** 颜色附件使用的 RGBA 浮点清除值。 */
typedef struct granit_clear_color_value {
  float red;
  float green;
  float blue;
  float alpha;
} granit_clear_color_value;

/** 深度/模板附件使用的清除值。 */
typedef struct granit_clear_depth_stencil_value {
  float depth;
  uint32_t stencil;
} granit_clear_depth_stencil_value;

/** 单个颜色 Texture View 的渲染行为。 */
typedef struct granit_color_attachment_desc {
  uint32_t struct_size;
  granit_attachment_load_operation load_operation;
  granit_attachment_store_operation store_operation;
  uint32_t reserved;
  granit_texture_view view;
  granit_clear_color_value clear_value;
  uint64_t reserved_2;
  /** 可选的单采样解析目标；仅用于多采样颜色附件。 */
  granit_texture_view resolve_view;
} granit_color_attachment_desc;
#define GRANIT_COLOR_ATTACHMENT_DESC_VERSION_1_SIZE                                                \
  ((uint32_t)(offsetof(granit_color_attachment_desc, resolve_view) + sizeof(granit_texture_view)))
#define GRANIT_COLOR_ATTACHMENT_DESC_INIT                                                          \
  {(uint32_t)sizeof(granit_color_attachment_desc),                                                 \
   GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR,                                                         \
   GRANIT_ATTACHMENT_STORE_OPERATION_STORE,                                                        \
   UINT32_C(0),                                                                                    \
   GRANIT_NULL_HANDLE,                                                                             \
   {0.0F, 0.0F, 0.0F, 1.0F},                                                                       \
   UINT64_C(0),                                                                                    \
   GRANIT_NULL_HANDLE}

/** 单个深度/模板 Texture View 的渲染行为。 */
typedef struct granit_depth_stencil_attachment_desc {
  uint32_t struct_size;
  granit_attachment_load_operation depth_load_operation;
  granit_attachment_store_operation depth_store_operation;
  granit_attachment_load_operation stencil_load_operation;
  granit_attachment_store_operation stencil_store_operation;
  uint32_t reserved;
  granit_texture_view view;
  granit_clear_depth_stencil_value clear_value;
  uint64_t reserved_2;
} granit_depth_stencil_attachment_desc;
#define GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_VERSION_1_SIZE UINT32_C(48)
#define GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT                                                  \
  {GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_VERSION_1_SIZE,                                            \
   GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR,                                                         \
   GRANIT_ATTACHMENT_STORE_OPERATION_STORE,                                                        \
   GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD,                                                       \
   GRANIT_ATTACHMENT_STORE_OPERATION_DISCARD,                                                      \
   UINT32_C(0),                                                                                    \
   GRANIT_NULL_HANDLE,                                                                             \
   {1.0F, UINT32_C(0)},                                                                            \
   UINT64_C(0)}

typedef struct granit_rendering_area {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
} granit_rendering_area;

/** Dynamic Rendering 作用域描述；所有指针只在调用期间借用。 */
typedef struct granit_rendering_desc {
  uint32_t struct_size;
  uint32_t color_attachment_count;
  const granit_color_attachment_desc* color_attachments;
  const granit_depth_stencil_attachment_desc* depth_stencil_attachment;
  granit_rendering_area area;
  uint32_t layer_count;
  uint32_t reserved;
  uint64_t reserved_2;
} granit_rendering_desc;
#define GRANIT_RENDERING_DESC_VERSION_1_SIZE                                                       \
  ((uint32_t)(offsetof(granit_rendering_desc, reserved_2) + sizeof(uint64_t)))
#define GRANIT_RENDERING_DESC_INIT                                                                 \
  {GRANIT_RENDERING_DESC_VERSION_1_SIZE,                                                           \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   0,                                                                                              \
   {UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)},                                           \
   UINT32_C(1),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT64_C(0)}

#endif
