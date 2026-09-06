// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_READBACK_BATCH_H_
#define GRANIT_READBACK_BATCH_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/async_operation.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/texture.h>

/** 一组有界 GPU 到 CPU 异步回读请求。零值表示无效句柄。 */
typedef granit_handle granit_readback_batch;

#define GRANIT_READBACK_BATCH_UNLIMITED UINT64_C(0)

typedef uint32_t granit_readback_result_type;
#define GRANIT_READBACK_RESULT_TYPE_BUFFER UINT32_C(1)
#define GRANIT_READBACK_RESULT_TYPE_TEXTURE UINT32_C(2)

typedef uint32_t granit_readback_layout;
/** 将 Texture 行距重排为紧密布局。 */
#define GRANIT_READBACK_LAYOUT_TIGHT UINT32_C(1)
/** 保留后端复制所需的行距；调用方必须读取结果元数据。 */
#define GRANIT_READBACK_LAYOUT_BACKEND UINT32_C(2)

typedef struct granit_readback_batch_desc {
  uint32_t struct_size;
  uint32_t flags;
  /** 允许单次提交产生的最大结果字节数；零表示不限制。 */
  uint64_t max_result_bytes;
  /** 允许排队的最大回读数；零表示不限制。 */
  uint32_t max_operation_count;
  granit_readback_layout texture_layout;
} granit_readback_batch_desc;

#define GRANIT_READBACK_BATCH_DESC_VERSION_1_SIZE UINT32_C(24)
#define GRANIT_READBACK_BATCH_DESC_INIT                                                        \
  {GRANIT_READBACK_BATCH_DESC_VERSION_1_SIZE, UINT32_C(0), UINT64_C(0), UINT32_C(0),          \
   GRANIT_READBACK_LAYOUT_TIGHT}

/** Readback Batch 当前排队状态；可用于在记录请求前实施背压。 */
typedef struct granit_readback_batch_info {
  uint32_t struct_size;
  uint32_t operation_count;
  uint64_t result_bytes;
  uint32_t max_operation_count;
  uint32_t reserved;
  uint64_t max_result_bytes;
} granit_readback_batch_info;

#define GRANIT_READBACK_BATCH_INFO_VERSION_1_SIZE UINT32_C(32)
#define GRANIT_READBACK_BATCH_INFO_INIT                                                        \
  {GRANIT_READBACK_BATCH_INFO_VERSION_1_SIZE, UINT32_C(0), UINT64_C(0), UINT32_C(0),          \
   UINT32_C(0), UINT64_C(0)}

/** 单个异步回读结果的布局；仅操作成功后可查询。 */
typedef struct granit_readback_result_info {
  uint32_t struct_size;
  granit_readback_result_type type;
  uint64_t required_size;
  granit_texture_format format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t array_layer_count;
  uint32_t bytes_per_row;
  uint32_t rows_per_image;
  uint32_t reserved[2];
} granit_readback_result_info;

#define GRANIT_READBACK_RESULT_INFO_VERSION_1_SIZE                                            \
  ((uint32_t)(offsetof(granit_readback_result_info, reserved) + sizeof(uint32_t) * 2))
#define GRANIT_READBACK_RESULT_INFO_INIT                                                       \
  {GRANIT_READBACK_RESULT_INFO_VERSION_1_SIZE, UINT32_C(0), UINT64_C(0),                       \
   GRANIT_TEXTURE_FORMAT_UNDEFINED, UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0),        \
   UINT32_C(0), UINT32_C(0), {UINT32_C(0), UINT32_C(0)}}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_readback_batch_create(granit_renderer renderer,
                                                       const granit_readback_batch_desc* desc,
                                                       granit_readback_batch* batch);
/** 记录 Buffer 区域；result_index 标识本次提交中对应结果。 */
GRANIT_API granit_result granit_readback_batch_read_buffer(
    granit_renderer renderer, granit_readback_batch batch, granit_buffer buffer, uint64_t offset,
    uint64_t size, uint32_t* result_index);
/** 记录 Texture 区域；当前仅接受可传输、单采样的非压缩颜色子资源。 */
GRANIT_API granit_result granit_readback_batch_read_texture(
    granit_renderer renderer, granit_readback_batch batch, granit_texture texture,
    const granit_texture_write_region* region, uint32_t* result_index);
GRANIT_API granit_result granit_readback_batch_get_info(granit_renderer renderer,
                                                        granit_readback_batch batch,
                                                        granit_readback_batch_info* info);
/** 非阻塞提交全部请求；成功后 Batch 立即变为空，可记录下一次提交。 */
GRANIT_API granit_result granit_readback_batch_submit_async(
    granit_renderer renderer, granit_readback_batch batch, granit_async_operation* operation);
/** 查询成功操作中的一个结果布局；未成功或类型不匹配时不返回数据。 */
GRANIT_API granit_result granit_readback_operation_get_result_info(
    granit_renderer renderer, granit_async_operation operation, uint32_t result_index,
    granit_readback_result_info* info);
/** 复制成功操作中的结果；data 为 NULL 时仅查询 required size。 */
GRANIT_API granit_result granit_readback_operation_copy_result(
    granit_renderer renderer, granit_async_operation operation, uint32_t result_index, void* data,
    uint64_t* data_size);
GRANIT_API granit_result granit_readback_batch_reset(granit_renderer renderer,
                                                      granit_readback_batch batch);
GRANIT_API granit_result granit_readback_batch_destroy(granit_renderer renderer,
                                                        granit_readback_batch batch);

#ifdef __cplusplus
}
#endif

#endif
