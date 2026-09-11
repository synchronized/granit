// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_LIBRARY_BUILDER_H_
#define GRANIT_SHADER_LIBRARY_BUILDER_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/shader_types.h>
#include <granit/tools/shader_tools_export.h>

/** 单个 Shader Object 输入路径。路径只需在调用期间有效。 */
typedef struct granit_shader_tools_library_object_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const char* path;
  uint64_t path_length;
} granit_shader_tools_library_object_desc;

#define GRANIT_SHADER_TOOLS_LIBRARY_OBJECT_DESC_INIT                                               \
  {(uint32_t)sizeof(granit_shader_tools_library_object_desc), UINT32_C(0), 0, UINT64_C(0)}

/** Shader Library 构建描述。数组和路径只需在调用期间有效。 */
typedef struct granit_shader_tools_library_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const granit_shader_tools_library_object_desc* objects;
  uint64_t object_count;
  granit_shader_backend_flags target_backends;
  uint32_t reserved2;
  const char* output_path;
  uint64_t output_path_length;
} granit_shader_tools_library_desc;

#define GRANIT_SHADER_TOOLS_LIBRARY_DESC_INIT                                                      \
  {(uint32_t)sizeof(granit_shader_tools_library_desc),                                             \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 链接 `.grshaderobj` 及其目标 sidecar；输出未变化时将 cache_hit 写为 1。
 * 描述无效或 Object 损坏返回 INVALID_ARGUMENT，写入失败返回 INITIALIZATION_FAILED。
 * 调用不保留输入视图，可由多个线程使用互不重叠的输出路径并发调用。
 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_build_library(
    const granit_shader_tools_library_desc* desc, uint32_t* cache_hit);

#ifdef __cplusplus
}
#endif

#endif
