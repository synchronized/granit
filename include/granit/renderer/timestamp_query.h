// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TIMESTAMP_QUERY_H_
#define GRANIT_TIMESTAMP_QUERY_H_

#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/renderer.h>

/** 固定容量的 GPU 时间戳查询池句柄。零值无效。 */
typedef granit_handle granit_timestamp_query_pool;

typedef uint32_t granit_timestamp_stage;
#define GRANIT_TIMESTAMP_STAGE_TOP UINT32_C(1)
#define GRANIT_TIMESTAMP_STAGE_DRAW UINT32_C(2)
#define GRANIT_TIMESTAMP_STAGE_BOTTOM UINT32_C(3)

typedef struct granit_timestamp_query_pool_desc {
  uint32_t struct_size;
  uint32_t query_count;
  uint64_t reserved;
} granit_timestamp_query_pool_desc;
#define GRANIT_TIMESTAMP_QUERY_POOL_DESC_VERSION_1_SIZE UINT32_C(16)

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API
    granit_result granit_timestamp_query_pool_create(granit_renderer renderer,
                                                     const granit_timestamp_query_pool_desc* desc,
                                                     granit_timestamp_query_pool* pool);
GRANIT_API granit_result granit_timestamp_query_pool_get_results(granit_renderer renderer,
                                                                 granit_timestamp_query_pool pool,
                                                                 uint32_t first_query,
                                                                 uint32_t query_count,
                                                                 uint64_t* nanoseconds);
GRANIT_API granit_result granit_timestamp_query_pool_destroy(granit_renderer renderer,
                                                             granit_timestamp_query_pool pool);
GRANIT_API granit_result granit_command_recorder_reset_timestamp_queries(
    granit_renderer renderer, granit_command_recorder recorder, granit_timestamp_query_pool pool,
    uint32_t first_query, uint32_t query_count);
GRANIT_API granit_result granit_command_recorder_write_timestamp(granit_renderer renderer,
                                                                 granit_command_recorder recorder,
                                                                 granit_timestamp_query_pool pool,
                                                                 granit_timestamp_stage stage,
                                                                 uint32_t query_index);

#ifdef __cplusplus
}
#endif

#endif
