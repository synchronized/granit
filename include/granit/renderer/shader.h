// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_H_
#define GRANIT_SHADER_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/shader_types.h>
#include <granit/core/types.h>
#include <granit/renderer/renderer.h>

/** 单个阶段入口对应的 Shader 句柄。零值无效。 */
typedef granit_handle granit_shader;

/** 单一格式 Shader 创建描述。代码与入口名称只需在创建调用期间有效。 */
typedef struct granit_shader_desc {
  uint32_t struct_size;
  granit_shader_stage stage;
  granit_shader_code_format code_format;
  uint32_t reserved;
  const void* code;
  uint64_t code_size;
  const char* entry_point;
  uint32_t entry_point_length;
  uint32_t reserved_2;
} granit_shader_desc;

#define GRANIT_SHADER_DESC_SIZE ((uint32_t)sizeof(granit_shader_desc))

#define GRANIT_SHADER_DESC_INIT                                                                    \
  {(uint32_t)sizeof(granit_shader_desc),                                                           \
   GRANIT_SHADER_STAGE_VERTEX,                                                                     \
   GRANIT_SHADER_CODE_FORMAT_SPIRV,                                                                \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   "main",                                                                                         \
   UINT32_C(4),                                                                                    \
   UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 从一份带格式标记的代码创建 Shader；格式与 Renderer 不匹配时返回 GRANIT_ERROR_UNSUPPORTED。
 * 函数返回后不再引用代码和入口名称内存。
 */
GRANIT_API granit_result granit_shader_create(granit_renderer renderer,
                                              const granit_shader_desc* desc,
                                              granit_shader* shader);
/** 销毁 Shader 并立即使公开句柄失效。 */
GRANIT_API granit_result granit_shader_destroy(granit_renderer renderer, granit_shader shader);

#ifdef __cplusplus
}
#endif

#endif
