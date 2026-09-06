// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_WARMUP_H_
#define GRANIT_PIPELINE_WARMUP_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/async_operation.h>
#include <granit/renderer/pipeline.h>

/** 有界 Pipeline 预热请求集合；结果进入 Renderer 私有缓存，不创建调用方资源。 */
typedef granit_handle granit_pipeline_warmup_batch;

typedef uint32_t granit_pipeline_warmup_type;
#define GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS UINT32_C(1)
#define GRANIT_PIPELINE_WARMUP_TYPE_COMPUTE UINT32_C(2)
#define GRANIT_PIPELINE_WARMUP_CACHE_KEY_SIZE UINT32_C(32)

typedef struct granit_pipeline_warmup_batch_desc {
  uint32_t struct_size;
  uint32_t flags;
  /** 最大条目数；零表示不限制。 */
  uint32_t max_operation_count;
  uint32_t reserved;
} granit_pipeline_warmup_batch_desc;

#define GRANIT_PIPELINE_WARMUP_BATCH_DESC_VERSION_1_SIZE UINT32_C(16)
#define GRANIT_PIPELINE_WARMUP_BATCH_DESC_INIT                                                  \
  {GRANIT_PIPELINE_WARMUP_BATCH_DESC_VERSION_1_SIZE, UINT32_C(0), UINT32_C(0), UINT32_C(0)}

typedef struct granit_pipeline_warmup_batch_info {
  uint32_t struct_size;
  uint32_t operation_count;
  uint32_t max_operation_count;
  uint32_t reserved;
} granit_pipeline_warmup_batch_info;

#define GRANIT_PIPELINE_WARMUP_BATCH_INFO_VERSION_1_SIZE UINT32_C(16)
#define GRANIT_PIPELINE_WARMUP_BATCH_INFO_INIT                                                  \
  {GRANIT_PIPELINE_WARMUP_BATCH_INFO_VERSION_1_SIZE, UINT32_C(0), UINT32_C(0), UINT32_C(0)}

/** 单项预热结果；result 失败不影响同批其他条目。 */
typedef struct granit_pipeline_warmup_result_info {
  uint32_t struct_size;
  granit_pipeline_warmup_type type;
  granit_result result;
  uint32_t cache_hit;
  uint8_t cache_key[GRANIT_PIPELINE_WARMUP_CACHE_KEY_SIZE];
} granit_pipeline_warmup_result_info;

#define GRANIT_PIPELINE_WARMUP_RESULT_INFO_VERSION_1_SIZE                                      \
  ((uint32_t)sizeof(granit_pipeline_warmup_result_info))
#define GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT                                                 \
  {GRANIT_PIPELINE_WARMUP_RESULT_INFO_VERSION_1_SIZE, UINT32_C(0), GRANIT_ERROR_NOT_READY,      \
   UINT32_C(0), {0}}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_pipeline_warmup_batch_create(
    granit_renderer renderer, const granit_pipeline_warmup_batch_desc* desc,
    granit_pipeline_warmup_batch* batch);
GRANIT_API granit_result granit_pipeline_warmup_batch_add_graphics(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    const granit_graphics_pipeline_desc* desc, uint32_t* result_index);
/**
 * 复制描述中的数组并加入批次。描述引用的 Layout 与 Shader 必须保持有效，直到异步操作结束。
 */
GRANIT_API granit_result granit_pipeline_warmup_batch_add_compute(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    const granit_compute_pipeline_desc* desc, uint32_t* result_index);
GRANIT_API granit_result granit_pipeline_warmup_batch_get_info(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    granit_pipeline_warmup_batch_info* info);
GRANIT_API granit_result granit_pipeline_warmup_batch_submit_async(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    granit_async_operation* operation);
/** 操作尚未完成对应条目时返回 GRANIT_ERROR_NOT_READY。 */
GRANIT_API granit_result granit_pipeline_warmup_operation_get_result(
    granit_renderer renderer, granit_async_operation operation, uint32_t result_index,
    granit_pipeline_warmup_result_info* info);
GRANIT_API granit_result granit_pipeline_warmup_batch_reset(granit_renderer renderer,
                                                            granit_pipeline_warmup_batch batch);
GRANIT_API granit_result granit_pipeline_warmup_batch_destroy(granit_renderer renderer,
                                                              granit_pipeline_warmup_batch batch);

#ifdef __cplusplus
}
#endif

#endif
