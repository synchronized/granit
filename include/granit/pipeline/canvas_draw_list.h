// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_CANVAS_DRAW_LIST_H_
#define GRANIT_PIPELINE_CANVAS_DRAW_LIST_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/pipeline/export.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/render_target.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/texture.h>

/** 可复用的逐帧二维 Canvas 绘制列表句柄。零值无效。 */
typedef granit_handle granit_canvas_draw_list;

/** Canvas 顶点包含二维位置、UV 和打包 RGBA8 UNORM 颜色；坐标空间由录制接口定义。 */
typedef struct granit_canvas_vertex {
  float x;
  float y;
  float u;
  float v;
  uint32_t color;
} granit_canvas_vertex;

/** 一项 Canvas 几何的借用资源与裁剪状态。 */
typedef struct granit_canvas_draw_state {
  granit_texture_view texture;
  granit_sampler sampler;
  granit_scissor scissor;
} granit_canvas_draw_state;

/** 批量几何中的一段索引及其绘制状态；索引范围相对于本次批量输入。 */
typedef struct granit_canvas_draw_range {
  uint32_t first_index;
  uint32_t index_count;
  granit_canvas_draw_state state;
} granit_canvas_draw_range;

/** 创建时预留容量；clear 清空内容但保留这些动态容量。 */
typedef struct granit_canvas_draw_list_desc {
  uint32_t struct_size;
  uint32_t initial_vertex_capacity;
  uint32_t initial_index_capacity;
  uint32_t initial_item_capacity;
  /** 动态几何上传槽数量，必须为 1～GRANIT_MAX_FRAMES_IN_FLIGHT。 */
  uint32_t frame_slot_count;
  uint32_t reserved[3];
} granit_canvas_draw_list_desc;

#define GRANIT_CANVAS_DRAW_LIST_DESC_VERSION_1_SIZE                                                \
  ((uint32_t)(offsetof(granit_canvas_draw_list_desc, reserved) + sizeof(uint32_t[3])))

#define GRANIT_CANVAS_DRAW_LIST_DESC_INIT                                                          \
  {                                                                                                \
    (uint32_t)sizeof(granit_canvas_draw_list_desc), UINT32_C(0), UINT32_C(0), UINT32_C(0),         \
        GRANIT_DEFAULT_FRAMES_IN_FLIGHT, {                                                         \
      UINT32_C(0), UINT32_C(0), UINT32_C(0)                                                        \
    }                                                                                              \
  }

/** 在当前坐标空间中追加轴对齐矩形的便捷输入。 */
typedef struct granit_canvas_rect_desc {
  uint32_t struct_size;
  float x;
  float y;
  float width;
  float height;
  float u0;
  float v0;
  float u1;
  float v1;
  uint32_t color;
  granit_canvas_draw_state state;
  uint32_t reserved[4];
} granit_canvas_rect_desc;

#define GRANIT_CANVAS_RECT_DESC_VERSION_1_SIZE                                                     \
  ((uint32_t)(offsetof(granit_canvas_rect_desc, reserved) + sizeof(uint32_t[4])))

#define GRANIT_CANVAS_RECT_DESC_INIT                                                               \
  {                                                                                                \
    (uint32_t)sizeof(granit_canvas_rect_desc), 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F,     \
        UINT32_C(0xffffffff), {GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, {0, 0, 0, 0}}, {            \
      UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                                           \
    }                                                                                              \
  }

/** 当前列表内容统计；batch_count 是相邻兼容项合并后的实际 Draw 数。 */
typedef struct granit_canvas_draw_list_stats {
  uint32_t struct_size;
  uint32_t vertex_count;
  uint32_t index_count;
  uint32_t item_count;
  uint32_t batch_count;
  uint32_t reserved[3];
} granit_canvas_draw_list_stats;

#define GRANIT_CANVAS_DRAW_LIST_STATS_VERSION_1_SIZE                                               \
  ((uint32_t)(offsetof(granit_canvas_draw_list_stats, reserved) + sizeof(uint32_t[3])))

/** 把 Canvas Draw List 录制到颜色目标所需的参数。坐标使用左上原点、Y 轴向下的像素单位。 */
typedef struct granit_canvas_record_desc {
  uint32_t struct_size;
  granit_texture_view color;
  granit_texture_format color_format;
  uint32_t width;
  uint32_t height;
  granit_attachment_load_operation load_operation;
  uint32_t encode_srgb;
  uint32_t frame_slot;
  uint32_t reserved[3];
} granit_canvas_record_desc;

#define GRANIT_CANVAS_RECORD_DESC_VERSION_1_SIZE ((uint32_t)sizeof(granit_canvas_record_desc))

#define GRANIT_CANVAS_FRAME_SLOT_AUTO UINT32_MAX

#define GRANIT_CANVAS_RECORD_DESC_INIT                                                             \
  {                                                                                                \
    (uint32_t)sizeof(granit_canvas_record_desc), GRANIT_NULL_HANDLE,                               \
        GRANIT_TEXTURE_FORMAT_UNDEFINED, UINT32_C(0), UINT32_C(0),                                 \
        GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD, UINT32_C(0), GRANIT_CANVAS_FRAME_SLOT_AUTO, {       \
      UINT32_C(0), UINT32_C(0), UINT32_C(0)                                                        \
    }                                                                                              \
  }

#define GRANIT_CANVAS_DRAW_LIST_STATS_INIT                                                         \
  {                                                                                                \
    (uint32_t)sizeof(granit_canvas_draw_list_stats), UINT32_C(0), UINT32_C(0), UINT32_C(0),        \
        UINT32_C(0), {                                                                             \
      UINT32_C(0), UINT32_C(0), UINT32_C(0)                                                        \
    }                                                                                              \
  }

#ifdef __cplusplus
extern "C" {
#endif

/** 创建与 Renderer 关联的可复用 Draw List。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_canvas_draw_list_create(granit_renderer renderer, const granit_canvas_draw_list_desc* desc,
                               granit_canvas_draw_list* list);

/** 清空当帧内容但保留已分配容量。调用方必须与 append 和读取操作进行外部同步。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_canvas_draw_list_clear(granit_renderer renderer, granit_canvas_draw_list list);

/**
 * 追加通用三角形几何。输入数组只在调用期间借用，索引相对本次 vertices 起点。
 * Texture View 与 Sampler 由调用方持有，并至少保持到列表完成录制。
 */
GRANIT_RENDER_PIPELINE_API granit_result granit_canvas_draw_list_append(
    granit_renderer renderer, granit_canvas_draw_list list, const granit_canvas_vertex* vertices,
    uint32_t vertex_count, const uint32_t* indices, uint32_t index_count,
    const granit_canvas_draw_state* state);

/**
 * 一次追加共享同一顶点和索引数组的多个绘制范围。输入仅在调用期间借用；indices
 * 相对于本次 vertices 起点，ranges 必须完整位于本次 indices 内且互不重叠。
 */
GRANIT_RENDER_PIPELINE_API granit_result granit_canvas_draw_list_append_batch(
    granit_renderer renderer, granit_canvas_draw_list list, const granit_canvas_vertex* vertices,
    uint32_t vertex_count, const uint32_t* indices, uint32_t index_count,
    const granit_canvas_draw_range* ranges, uint32_t range_count);

/** 追加两个三角形组成的轴对齐矩形。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_canvas_draw_list_append_rect(
    granit_renderer renderer, granit_canvas_draw_list list, const granit_canvas_rect_desc* desc);

/** 查询列表内容与合批后的 Draw 数量。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_canvas_draw_list_get_stats(
    granit_renderer renderer, granit_canvas_draw_list list, granit_canvas_draw_list_stats* stats);

/**
 * 将整个列表录制到已经 begin 的 Command Recorder。函数不结束或提交 Recorder。
 * 列表、目标及其借用资源必须属于同一 Renderer，并保持有效至 Recorder 完成。
 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_canvas_draw_list_record(granit_renderer renderer, granit_command_recorder recorder,
                               granit_canvas_draw_list list, const granit_canvas_record_desc* desc);

/** 销毁 Draw List，不销毁其借用的 Texture View 或 Sampler。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_canvas_draw_list_destroy(granit_renderer renderer, granit_canvas_draw_list list);

#ifdef __cplusplus
}
#endif

#endif
