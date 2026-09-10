// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MATERIAL_H_
#define GRANIT_PIPELINE_MATERIAL_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/pipeline/export.h>
#include <granit/renderer/pipeline_warmup.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/shader_library.h>

/** 已加载材质包及其 GPU 实例的句柄。零值无效。 */
typedef granit_handle granit_material;

typedef uint32_t granit_material_parameter_type;
#define GRANIT_MATERIAL_PARAMETER_BOOL32 UINT32_C(0)
#define GRANIT_MATERIAL_PARAMETER_INT32 UINT32_C(1)
#define GRANIT_MATERIAL_PARAMETER_UINT32 UINT32_C(2)
#define GRANIT_MATERIAL_PARAMETER_FLOAT32 UINT32_C(3)
#define GRANIT_MATERIAL_PARAMETER_FLOAT2 UINT32_C(4)
#define GRANIT_MATERIAL_PARAMETER_FLOAT3 UINT32_C(5)
#define GRANIT_MATERIAL_PARAMETER_FLOAT4 UINT32_C(6)
#define GRANIT_MATERIAL_PARAMETER_MATRIX4 UINT32_C(7)
#define GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW UINT32_C(8)
#define GRANIT_MATERIAL_PARAMETER_SAMPLER UINT32_C(9)

/** 一项参数更新。值参数使用 data/size，资源参数使用 resource。 */
typedef struct granit_material_parameter_update {
  uint64_t id;
  granit_material_parameter_type type;
  uint32_t reserved;
  const void* data;
  uint64_t size;
  granit_handle resource;
} granit_material_parameter_update;

typedef struct granit_material_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const void* archive_data;
  uint64_t archive_size;
  const granit_material_parameter_update* initial_updates;
  uint32_t initial_update_count;
  uint32_t reserved_tail;
  granit_shader_library shader_library;
  uint64_t reserved_2;
} granit_material_desc;

/** 将材质变体加入 Pipeline 预热批次所需的目标描述。variant 为零时选择该 Pass 的首个变体。 */
typedef struct granit_material_pipeline_warmup_desc {
  uint32_t struct_size;
  uint32_t reserved;
  uint64_t pass;
  uint64_t variant;
  granit_texture_format color_format;
  granit_texture_format depth_stencil_format;
  granit_sample_count sample_count;
  uint32_t reserved_tail;
} granit_material_pipeline_warmup_desc;

#define GRANIT_MATERIAL_PIPELINE_WARMUP_DESC_INIT                                                  \
  {(uint32_t)sizeof(granit_material_pipeline_warmup_desc),                                         \
   UINT32_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   GRANIT_SAMPLE_COUNT_1,                                                                          \
   UINT32_C(0)}

#define GRANIT_MATERIAL_DESC_VERSION_1_SIZE ((uint32_t)sizeof(granit_material_desc))

#define GRANIT_MATERIAL_DESC_INIT                                                                  \
  {(uint32_t)sizeof(granit_material_desc),                                                         \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   GRANIT_NULL_HANDLE,                                                                             \
   UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 使用与材质编译器一致的稳定算法计算参数 ID；名称在调用期间借用。 */
GRANIT_RENDER_PIPELINE_API uint64_t granit_material_parameter_id(const char* name,
                                                                 uint32_t name_length);

/**
 * 从材质归档创建 GPU 实例，并事务式应用初始参数。
 *
 * 归档和更新数组只在调用期间借用。成功后材质由调用者销毁；失败时输出句柄为零。
 */
GRANIT_RENDER_PIPELINE_API granit_result granit_material_create(granit_renderer renderer,
                                                                const granit_material_desc* desc,
                                                                granit_material* material);

/**
 * 事务式应用一批参数并刷新 GPU 状态。
 *
 * 任一参数或 GPU 操作失败时，原材质保持不变。同一材质的更新不可并发。
 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_material_update(granit_renderer renderer, granit_material material,
                       const granit_material_parameter_update* updates, uint32_t update_count);

/**
 * 将材质变体使用的 Graphics Pipeline 加入批次；结果索引可用于查询逐项错误。
 * 材质和批次必须属于同一 Renderer，且材质至少存活到批次提交完成。
 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_material_add_pipeline_warmup(granit_renderer renderer, granit_material material,
                                    const granit_material_pipeline_warmup_desc* desc,
                                    granit_pipeline_warmup_batch batch, uint32_t* result_index);

/** 销毁材质并使旧句柄立即失效；不得与同一材质的更新并发。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_material_destroy(granit_renderer renderer,
                                                                 granit_material material);

#ifdef __cplusplus
}
#endif

#endif
