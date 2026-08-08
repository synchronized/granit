// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BUFFER_H_
#define GRANIT_BUFFER_H_

#include <stdint.h>

#include <granit/export.h>
#include <granit/renderer.h>
#include <granit/resource_types.h>
#include <granit/result.h>
#include <granit/types.h>

/** Buffer 资源句柄。零值无效。 */
typedef granit_handle granit_buffer;

#ifdef __cplusplus
extern "C" {
#endif

/** 创建不含初始数据的 Buffer。 */
GRANIT_API granit_result granit_buffer_create(granit_renderer renderer,
                                              const granit_buffer_desc* desc,
                                              granit_buffer* buffer);

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
