// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DEBUG_DRAW_LIST_H_
#define GRANIT_PIPELINE_DEBUG_DRAW_LIST_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/pipeline/export.h>
#include <granit/renderer/renderer.h>

/** 可复用的逐帧调试绘制命令列表句柄。零值无效。 */
typedef granit_handle granit_debug_draw_list;

typedef uint32_t granit_debug_draw_space;
#define GRANIT_DEBUG_DRAW_SPACE_WORLD UINT32_C(0)
#define GRANIT_DEBUG_DRAW_SPACE_SCREEN UINT32_C(1)

typedef uint32_t granit_debug_draw_depth_mode;
#define GRANIT_DEBUG_DRAW_DEPTH_MODE_DISABLED UINT32_C(0)
#define GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST UINT32_C(1)

/** 调试图元顶点；颜色是打包 RGBA8 UNORM。 */
typedef struct granit_debug_draw_vertex {
  float x;
  float y;
  float z;
  uint32_t color;
} granit_debug_draw_vertex;

/** 一条线段；width 为像素宽度且必须大于零。 */
typedef struct granit_debug_draw_line {
  granit_debug_draw_vertex start;
  granit_debug_draw_vertex end;
  float width;
  granit_debug_draw_space space;
  granit_debug_draw_depth_mode depth_mode;
  uint32_t reserved;
} granit_debug_draw_line;

/** 一个调试三角形。 */
typedef struct granit_debug_draw_triangle {
  granit_debug_draw_vertex vertices[3];
  granit_debug_draw_space space;
  granit_debug_draw_depth_mode depth_mode;
  uint32_t reserved[2];
} granit_debug_draw_triangle;

typedef struct granit_debug_draw_list_desc {
  uint32_t struct_size;
  uint32_t initial_line_capacity;
  uint32_t initial_triangle_capacity;
  uint32_t reserved[5];
} granit_debug_draw_list_desc;

#define GRANIT_DEBUG_DRAW_LIST_DESC_INIT                                                           \
  {                                                                                                \
    (uint32_t)sizeof(granit_debug_draw_list_desc), UINT32_C(0), UINT32_C(0), {                     \
      UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                              \
    }                                                                                              \
  }

typedef struct granit_debug_draw_list_stats {
  uint32_t struct_size;
  uint32_t line_count;
  uint32_t triangle_count;
  uint32_t reserved[5];
} granit_debug_draw_list_stats;

#define GRANIT_DEBUG_DRAW_LIST_STATS_INIT                                                          \
  {                                                                                                \
    (uint32_t)sizeof(granit_debug_draw_list_stats), UINT32_C(0), UINT32_C(0), {                    \
      UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                              \
    }                                                                                              \
  }

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_RENDER_PIPELINE_API
    granit_result granit_debug_draw_list_create(granit_renderer renderer,
                                                const granit_debug_draw_list_desc* desc,
                                                granit_debug_draw_list* list);
GRANIT_RENDER_PIPELINE_API granit_result granit_debug_draw_list_clear(granit_renderer renderer,
                                                                      granit_debug_draw_list list);
/** 批量复制线段命令；输入数组只在调用期间借用。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_debug_draw_list_append_lines(granit_renderer renderer, granit_debug_draw_list list,
                                    const granit_debug_draw_line* lines, uint32_t line_count);
/** 批量复制三角形命令；输入数组只在调用期间借用。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_debug_draw_list_append_triangles(
    granit_renderer renderer, granit_debug_draw_list list,
    const granit_debug_draw_triangle* triangles, uint32_t triangle_count);
GRANIT_RENDER_PIPELINE_API granit_result granit_debug_draw_list_get_stats(
    granit_renderer renderer, granit_debug_draw_list list, granit_debug_draw_list_stats* stats);
GRANIT_RENDER_PIPELINE_API granit_result
granit_debug_draw_list_destroy(granit_renderer renderer, granit_debug_draw_list list);

#ifdef __cplusplus
}
#endif

#endif
