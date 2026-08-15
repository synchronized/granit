// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_TEXT_ATLAS_H_
#define GRANIT_PIPELINE_TEXT_ATLAS_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/pipeline/export.h>
#include <granit/renderer/renderer.h>

/** 与 Renderer 关联的 R8 字形 Atlas 缓存句柄。零值无效。 */
typedef granit_handle granit_text_atlas;

typedef struct granit_text_atlas_desc {
  uint32_t struct_size;
  uint32_t page_width;
  uint32_t page_height;
  uint32_t max_pages;
  uint32_t padding;
  uint32_t reserved[3];
} granit_text_atlas_desc;

#define GRANIT_TEXT_ATLAS_DESC_INIT                                                                \
  {                                                                                                \
    (uint32_t)sizeof(granit_text_atlas_desc), UINT32_C(512), UINT32_C(512), UINT32_C(8),           \
        UINT32_C(1), {                                                                             \
      UINT32_C(0), UINT32_C(0), UINT32_C(0)                                                        \
    }                                                                                              \
  }

/**
 * 调用方栅格化的单通道覆盖率字形。font_key 与 glyph_id 共同组成缓存键。
 * bitmap 采用逐行 R8 数据；零宽且零高表示无可见像素的字形，此时 bitmap 必须为空。
 */
typedef struct granit_text_glyph_bitmap_desc {
  uint32_t struct_size;
  uint32_t glyph_id;
  uint64_t font_key;
  uint32_t width;
  uint32_t height;
  float bearing_x;
  float bearing_y;
  const void* bitmap;
  uint64_t bitmap_size;
  uint32_t bytes_per_row;
  uint32_t reserved[3];
} granit_text_glyph_bitmap_desc;

#define GRANIT_TEXT_GLYPH_BITMAP_DESC_INIT                                                         \
  {                                                                                                \
    (uint32_t)sizeof(granit_text_glyph_bitmap_desc), UINT32_C(0), UINT64_C(0), UINT32_C(0),        \
        UINT32_C(0), 0.0F, 0.0F, 0, UINT64_C(0), UINT32_C(0), {                                    \
      UINT32_C(0), UINT32_C(0), UINT32_C(0)                                                        \
    }                                                                                              \
  }

typedef struct granit_text_atlas_stats {
  uint32_t struct_size;
  uint32_t glyph_count;
  uint32_t page_count;
  uint32_t reserved[5];
} granit_text_atlas_stats;

#define GRANIT_TEXT_ATLAS_STATS_INIT                                                               \
  {                                                                                                \
    (uint32_t)sizeof(granit_text_atlas_stats), UINT32_C(0), UINT32_C(0), {                         \
      UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                              \
    }                                                                                              \
  }

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_RENDER_PIPELINE_API granit_result granit_text_atlas_create(
    granit_renderer renderer, const granit_text_atlas_desc* desc, granit_text_atlas* atlas);
/**
 * 上传并缓存一个字形。重复键只允许尺寸与 bearing 完全相同，并原位更新覆盖率数据。
 * 函数返回后不再借用 bitmap。
 */
GRANIT_RENDER_PIPELINE_API granit_result granit_text_atlas_upload_glyph(
    granit_renderer renderer, granit_text_atlas atlas, const granit_text_glyph_bitmap_desc* glyph);
GRANIT_RENDER_PIPELINE_API granit_result granit_text_atlas_get_stats(
    granit_renderer renderer, granit_text_atlas atlas, granit_text_atlas_stats* stats);
GRANIT_RENDER_PIPELINE_API granit_result granit_text_atlas_destroy(granit_renderer renderer,
                                                                   granit_text_atlas atlas);

#ifdef __cplusplus
}
#endif

#endif
