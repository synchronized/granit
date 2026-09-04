// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_H_
#define GRANIT_SHADER_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/renderer.h>

/** 单个阶段入口对应的 Shader 句柄。零值无效。 */
typedef granit_handle granit_shader;
typedef uint32_t granit_shader_stage;

#define GRANIT_SHADER_STAGE_VERTEX UINT32_C(1)
#define GRANIT_SHADER_STAGE_FRAGMENT UINT32_C(2)
#define GRANIT_SHADER_STAGE_COMPUTE UINT32_C(3)

/** 跨后端 Shader 创建描述。SPIR-V 与 WGSL 输入内存只需在创建调用期间有效。 */
typedef struct granit_shader_desc {
  uint32_t struct_size;
  granit_shader_stage stage;
  const void* code;
  uint64_t code_size;
  const char* entry_point;
  uint32_t entry_point_length;
  uint32_t reserved;
  const char* wgsl;
  uint64_t wgsl_length;
} granit_shader_desc;

#define GRANIT_SHADER_DESC_SIZE                                                                    \
  ((uint32_t)(offsetof(granit_shader_desc, wgsl_length) + sizeof(uint64_t)))

#define GRANIT_SHADER_DESC_INIT                                                                    \
  {(uint32_t)sizeof(granit_shader_desc),                                                           \
   GRANIT_SHADER_STAGE_VERTEX,                                                                     \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   "main",                                                                                         \
   UINT32_C(4),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 创建 Shader；Vulkan 使用 SPIR-V，WebGPU 使用 WGSL，函数返回后不再引用输入内存。 */
GRANIT_API granit_result granit_shader_create(granit_renderer renderer,
                                              const granit_shader_desc* desc,
                                              granit_shader* shader);
/** 销毁 Shader 并立即使公开句柄失效。 */
GRANIT_API granit_result granit_shader_destroy(granit_renderer renderer, granit_shader shader);

#ifdef __cplusplus
}
#endif

#endif
