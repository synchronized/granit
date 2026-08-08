// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BUFFER_H_
#define GRANIT_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/export.h>
#include <granit/renderer.h>
#include <granit/resource_types.h>
#include <granit/result.h>
#include <granit/types.h>

/** Buffer 资源句柄。零值无效。 */
typedef granit_handle granit_buffer;

/** 创建 Buffer 时使用的完整初始数据。数据只在调用期间借用。 */
typedef struct granit_buffer_initial_data {
  uint32_t struct_size;
  uint32_t reserved;
  const void* data;
  uint64_t size;
} granit_buffer_initial_data;

#define GRANIT_BUFFER_INITIAL_DATA_VERSION_1_SIZE                                                  \
  ((uint32_t)(offsetof(granit_buffer_initial_data, size) + sizeof(uint64_t)))
#define GRANIT_BUFFER_INITIAL_DATA_INIT                                                            \
  {GRANIT_BUFFER_INITIAL_DATA_VERSION_1_SIZE, UINT32_C(0), NULL, UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 创建不含初始数据的 Buffer。 */
GRANIT_API granit_result granit_buffer_create(granit_renderer renderer,
                                              const granit_buffer_desc* desc,
                                              granit_buffer* buffer);

/** 创建 Buffer 并同步写入覆盖完整 Buffer 的初始数据。 */
GRANIT_API granit_result granit_buffer_create_with_data(
    granit_renderer renderer, const granit_buffer_desc* desc,
    const granit_buffer_initial_data* initial_data, granit_buffer* buffer);

/** 同步写入 Buffer 的指定范围；不适用于 READBACK Buffer。 */
GRANIT_API granit_result granit_buffer_write(granit_renderer renderer, granit_buffer buffer,
                                             uint64_t offset, const void* data, uint64_t size);

/** 映射 UPLOAD 或 READBACK Buffer 的指定非空范围。 */
GRANIT_API granit_result granit_buffer_map(granit_renderer renderer, granit_buffer buffer,
                                           uint64_t offset, uint64_t size, void** data);

/** 结束当前映射；UPLOAD Buffer 会自动刷新写入范围。 */
GRANIT_API granit_result granit_buffer_unmap(granit_renderer renderer, granit_buffer buffer);

/** 销毁 Buffer，并使句柄立即失效。 */
GRANIT_API granit_result granit_buffer_destroy(granit_renderer renderer, granit_buffer buffer);

#ifdef __cplusplus
}
#endif

#endif
