// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_RENDER_PIPELINE_H_
#define GRANIT_PIPELINE_RENDER_PIPELINE_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/pipeline/export.h>
#include <granit/pipeline/material.h>
#include <granit/pipeline/mesh.h>
#include <granit/pipeline/scene.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/resource_types.h>
#include <granit/renderer/texture.h>

/** 统一参考渲染管线句柄。零值无效。 */
typedef granit_handle granit_render_pipeline;

typedef uint32_t granit_render_pipeline_stage;
#define GRANIT_RENDER_PIPELINE_STAGE_OPAQUE UINT32_C(0)
#define GRANIT_RENDER_PIPELINE_STAGE_SHADOW UINT32_C(1)
#define GRANIT_RENDER_PIPELINE_STAGE_OVERLAY UINT32_C(2)

/** Scene payload 对应的一项 Granit Mesh 与 Material 关联。 */
typedef struct granit_render_pipeline_draw_binding {
  uint64_t payload;
  granit_mesh mesh;
  granit_material material;
  uint64_t reserved;
} granit_render_pipeline_draw_binding;

/** 固定阶段录制回调的只读上下文；所有指针和 Texture View 只在回调期间有效。 */
typedef struct granit_render_pipeline_record_info {
  uint32_t struct_size;
  granit_render_pipeline_stage stage;
  granit_command_recorder recorder;
  granit_texture_view color_input;
  granit_texture_view color_output;
  granit_texture_view depth_output;
  granit_texture_view shadow_input;
  granit_texture_view ibl_irradiance;
  granit_texture_view ibl_prefiltered_environment;
  granit_texture_view ibl_brdf_lut;
  granit_bind_group_layout ibl_layout;
  granit_bind_group ibl_group;
  uint32_t view_index;
  uint32_t payload_count;
  const uint64_t* payloads;
  const granit_render_pipeline_draw_binding* draw_bindings;
  const granit_scene_view* view;
  const granit_scene_renderable* renderables;
  granit_matrix4 light_view_projection;
  float exposure_scale;
  uint32_t encode_srgb;
  uint32_t reserved[2];
} granit_render_pipeline_record_info;

typedef granit_result (*granit_render_pipeline_record_callback)(
    const granit_render_pipeline_record_info* info, void* user_data);

typedef struct granit_render_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_render_pipeline_record_callback record;
  void* user_data;
} granit_render_pipeline_desc;

#define GRANIT_RENDER_PIPELINE_DESC_INIT                                                           \
  {(uint32_t)sizeof(granit_render_pipeline_desc), UINT32_C(0), 0, 0}

/** 多 View 渲染中单个 View 的独立输出。 */
typedef struct granit_render_pipeline_output {
  uint32_t struct_size;
  uint32_t reserved;
  granit_texture_view view;
  granit_texture_format format;
  uint32_t width;
  uint32_t height;
  uint32_t reserved_tail;
} granit_render_pipeline_output;

#define GRANIT_RENDER_PIPELINE_OUTPUT_INIT                                                         \
  {(uint32_t)sizeof(granit_render_pipeline_output),                                                \
   UINT32_C(0),                                                                                    \
   GRANIT_NULL_HANDLE,                                                                             \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0)}

typedef struct granit_render_pipeline_render_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_scene_snapshot scene;
  granit_texture_view output;
  granit_texture_format output_format;
  uint32_t width;
  uint32_t height;
  uint32_t first_view;
  uint32_t view_count;
  float exposure_ev;
  uint32_t draw_binding_count;
  const granit_render_pipeline_draw_binding* draw_bindings;
  /** 多 View 独立输出数组；单 View 可保持为空并使用上方 output/format/width/height。 */
  uint32_t output_count;
  const granit_render_pipeline_output* outputs;
  /** 可选 Swapchain Frame；非零时本次只允许渲染一个 View，并使用帧同步提交。 */
  granit_frame frame;
  uint32_t reserved_tail;
} granit_render_pipeline_render_desc;

#define GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT                                                    \
  {(uint32_t)sizeof(granit_render_pipeline_render_desc),                                           \
   UINT32_C(0),                                                                                    \
   GRANIT_NULL_HANDLE,                                                                             \
   GRANIT_NULL_HANDLE,                                                                             \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(1),                                                                                    \
   0.0F,                                                                                           \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   GRANIT_NULL_HANDLE,                                                                             \
   UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 创建借用 Renderer 的参考渲染管线；可选回调及 user_data 必须保持有效直至销毁。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_render_pipeline_create(granit_renderer renderer, const granit_render_pipeline_desc* desc,
                              granit_render_pipeline* pipeline);

/**
 * 为指定 View 范围构建并提交固定的 PBR HDR 与 Tone Mapping 图。
 *
 * 每个可见 Renderable 的 payload 必须在 draw_bindings 中唯一对应一项。Mesh 和 Material
 * 必须属于当前 Renderer，且在调用期间不得更新或销毁。
 * 单 View 可使用 output/format/width/height；多 View 必须提供与 view_count 等长的 outputs 数组。
 * frame 为零时执行普通离屏提交；frame 非零时使用 Swapchain 帧提交，并要求 view_count 为 1。
 * Overlay 阶段在 Tone Mapping 后执行，color_input 与 color_output 指向同一显示空间目标；回调必须
 * 使用 LOAD 保留已有内容。该阶段 payload_count 为零，encode_srgb 表示 UNORM 输出是否需要 Shader
 * 编码。同一管线不可并发调用。回调不得结束、提交或销毁传入的 Recorder，也不得递归调用本管线。
 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_render_pipeline_render(granit_renderer renderer, granit_render_pipeline pipeline,
                              const granit_render_pipeline_render_desc* desc);

/** 销毁管线并使旧句柄立即失效；不得与 render 并发。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_render_pipeline_destroy(granit_renderer renderer, granit_render_pipeline pipeline);

#ifdef __cplusplus
}
#endif

#endif
