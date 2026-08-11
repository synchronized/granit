// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_UPLOAD_BATCH_H_
#define GRANIT_UPLOAD_BATCH_H_

#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/texture.h>

/** 一组同步批量上传操作。零值表示无效句柄。 */
typedef granit_handle granit_upload_batch;

typedef struct granit_upload_batch_desc {
  uint32_t struct_size;
  uint32_t flags;
  uint64_t reserved;
} granit_upload_batch_desc;

#define GRANIT_UPLOAD_BATCH_DESC_VERSION_1_SIZE UINT32_C(16)
#define GRANIT_UPLOAD_BATCH_DESC_INIT                                                              \
  {GRANIT_UPLOAD_BATCH_DESC_VERSION_1_SIZE, UINT32_C(0), UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_upload_batch_create(granit_renderer renderer,
                                                    const granit_upload_batch_desc* desc,
                                                    granit_upload_batch* batch);
GRANIT_API granit_result granit_upload_batch_write_buffer(granit_renderer renderer,
                                                          granit_upload_batch batch,
                                                          granit_buffer buffer, uint64_t offset,
                                                          const void* data, uint64_t size);
GRANIT_API granit_result granit_upload_batch_write_texture(
    granit_renderer renderer, granit_upload_batch batch, granit_texture texture, const void* data,
    uint64_t size, const granit_texture_data_layout* layout,
    const granit_texture_write_region* region);
/** 同步提交全部写入；成功返回时 GPU 复制已经完成，Batch 可立即复用。 */
GRANIT_API granit_result granit_upload_batch_submit(granit_renderer renderer,
                                                    granit_upload_batch batch);
/** 丢弃尚未提交的全部写入。 */
GRANIT_API granit_result granit_upload_batch_reset(granit_renderer renderer,
                                                   granit_upload_batch batch);
GRANIT_API granit_result granit_upload_batch_destroy(granit_renderer renderer,
                                                     granit_upload_batch batch);

#ifdef __cplusplus
}
#endif

#endif
