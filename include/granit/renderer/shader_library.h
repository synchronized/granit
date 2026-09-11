// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_LIBRARY_H_
#define GRANIT_SHADER_LIBRARY_H_

#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/shader.h>

/** 属于 Renderer 的 Shader Library 句柄。零值无效。 */
typedef granit_handle granit_shader_library;

/**
 * Shader Library 创建描述。
 *
 * Granit 借用 archive_data 指向的完整 `.grshlib` 字节直到 Library 销毁。调用者必须保证这段
 * 内存的地址和内容在此期间保持有效且不变。
 */
typedef struct granit_shader_library_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const void* archive_data;
  uint64_t archive_size;
} granit_shader_library_desc;

#define GRANIT_SHADER_LIBRARY_DESC_VERSION_1_SIZE ((uint32_t)sizeof(granit_shader_library_desc))
#define GRANIT_SHADER_LIBRARY_DESC_INIT                                                            \
  {(uint32_t)sizeof(granit_shader_library_desc), UINT32_C(0), 0, UINT64_C(0)}

/** 已验证 Shader Library 的稳定摘要。 */
typedef struct granit_shader_library_info {
  uint32_t struct_size;
  granit_shader_backend_flags backend_flags;
  granit_shader_digest content_digest;
  uint32_t shader_count;
  uint32_t variant_count;
  uint32_t payload_count;
  uint32_t reserved;
  uint64_t archive_size;
} granit_shader_library_info;

#define GRANIT_SHADER_LIBRARY_INFO_VERSION_1_SIZE ((uint32_t)sizeof(granit_shader_library_info))
#define GRANIT_SHADER_LIBRARY_INFO_INIT                                                            \
  {(uint32_t)sizeof(granit_shader_library_info),                                                   \
   UINT32_C(0),                                                                                    \
   {0},                                                                                            \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 校验内存中的 `.grshlib` 并返回摘要；函数返回后不再引用输入内存。 */
GRANIT_API granit_result granit_shader_library_inspect(const void* archive_data,
                                                       uint64_t archive_size,
                                                       granit_shader_library_info* info);
/** 创建属于 Renderer 的 Shader Library；成功后持续借用描述中的归档字节。 */
GRANIT_API granit_result granit_shader_library_create(granit_renderer renderer,
                                                      const granit_shader_library_desc* desc,
                                                      granit_shader_library* library);
/** 返回已创建 Shader Library 的摘要。 */
GRANIT_API granit_result granit_shader_library_get_info(granit_renderer renderer,
                                                        granit_shader_library library,
                                                        granit_shader_library_info* info);
/**
 * 按内容 ID 选择当前 Renderer 支持的变体并取得 Shader。
 *
 * 成功返回的 Shader 由调用者通过 granit_shader_destroy 销毁。相同 Library 与内容 ID 复用同一个
 * 后端 Shader。Shader 或引用它的 Pipeline 存活期间，销毁 Library 返回
 * GRANIT_ERROR_RESOURCE_IN_USE。
 */
GRANIT_API granit_result
granit_shader_create_from_library(granit_renderer renderer, granit_shader_library library,
                                  const granit_shader_content_id content_id, granit_shader* shader);
/** 销毁 Shader Library 并立即使公开句柄失效。 */
GRANIT_API granit_result granit_shader_library_destroy(granit_renderer renderer,
                                                       granit_shader_library library);

#ifdef __cplusplus
}
#endif

#endif
