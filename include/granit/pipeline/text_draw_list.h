// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_TEXT_DRAW_LIST_H_
#define GRANIT_PIPELINE_TEXT_DRAW_LIST_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/pipeline/export.h>
#include <granit/pipeline/canvas_draw_list.h>
#include <granit/pipeline/text_atlas.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/renderer.h>

/** 可复用的逐帧已整形字形列表句柄。零值无效。 */
typedef granit_handle granit_text_draw_list;

/**
 * 一个已定位字形。font_key 由调用方定义，通常标识字体、字号和栅格化参数的组合。
 * x、y 是基线坐标；glyph_id 是整形器输出的字体内字形编号，允许为零。
 */
typedef struct granit_text_glyph_instance {
  uint64_t font_key;
  uint32_t glyph_id;
  uint32_t color;
  float x;
  float y;
  uint32_t reserved[2];
} granit_text_glyph_instance;

/** 一次批量追加的已整形字形序列；scissor 全零表示不裁剪。 */
typedef struct granit_text_glyph_run_desc {
  uint32_t struct_size;
  uint32_t glyph_count;
  const granit_text_glyph_instance* glyphs;
  granit_scissor scissor;
  uint32_t reserved[4];
} granit_text_glyph_run_desc;

#define GRANIT_TEXT_GLYPH_RUN_DESC_INIT                                                            \
  {                                                                                                \
    (uint32_t)sizeof(granit_text_glyph_run_desc), UINT32_C(0), 0, {0, 0, 0, 0}, {                  \
      UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                                           \
    }                                                                                              \
  }

typedef struct granit_text_draw_list_desc {
  uint32_t struct_size;
  uint32_t initial_glyph_capacity;
  uint32_t initial_run_capacity;
  uint32_t reserved[5];
} granit_text_draw_list_desc;

#define GRANIT_TEXT_DRAW_LIST_DESC_INIT                                                            \
  {                                                                                                \
    (uint32_t)sizeof(granit_text_draw_list_desc), UINT32_C(0), UINT32_C(0), {                      \
      UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                              \
    }                                                                                              \
  }

typedef struct granit_text_draw_list_stats {
  uint32_t struct_size;
  uint32_t glyph_count;
  uint32_t run_count;
  uint32_t reserved[5];
} granit_text_draw_list_stats;

#define GRANIT_TEXT_DRAW_LIST_STATS_INIT                                                           \
  {                                                                                                \
    (uint32_t)sizeof(granit_text_draw_list_stats), UINT32_C(0), UINT32_C(0), {                     \
      UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                              \
    }                                                                                              \
  }

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_RENDER_PIPELINE_API granit_result granit_text_draw_list_create(
    granit_renderer renderer, const granit_text_draw_list_desc* desc, granit_text_draw_list* list);
/** 清空字形和 Run 但保留容量；同一列表由调用方进行外部同步。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_text_draw_list_clear(granit_renderer renderer,
                                                                     granit_text_draw_list list);
/** 批量复制调用方已经完成整形和定位的字形；函数返回后不再借用输入数组。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_text_draw_list_append_glyph_run(
    granit_renderer renderer, granit_text_draw_list list, const granit_text_glyph_run_desc* run);
GRANIT_RENDER_PIPELINE_API granit_result granit_text_draw_list_get_stats(
    granit_renderer renderer, granit_text_draw_list list, granit_text_draw_list_stats* stats);
/**
 * 将已上传字形转换并追加到 Canvas；基线坐标使用 Canvas 的左上原点、Y 轴向下空间。
 * 字形缺失时返回 NOT_READY，且调用方不应依赖失败前已经追加的部分内容。
 */
GRANIT_RENDER_PIPELINE_API granit_result granit_text_draw_list_append_to_canvas(
    granit_renderer renderer, granit_text_draw_list list, granit_text_atlas atlas,
    granit_canvas_draw_list canvas);
GRANIT_RENDER_PIPELINE_API granit_result granit_text_draw_list_destroy(granit_renderer renderer,
                                                                       granit_text_draw_list list);

#ifdef __cplusplus
}
#endif

#endif
