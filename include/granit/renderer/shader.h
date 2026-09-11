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

typedef uint32_t granit_shader_code_format;
#define GRANIT_SHADER_CODE_FORMAT_WGSL UINT32_C(1)
#define GRANIT_SHADER_CODE_FORMAT_SPIRV UINT32_C(2)
#define GRANIT_SHADER_DIGEST_SIZE UINT32_C(32)
#define GRANIT_SHADER_ASSET_MAX_VARIANTS UINT32_C(2)

/** Shader 资产及 Library 使用的固定长度摘要。 */
typedef uint8_t granit_shader_digest[GRANIT_SHADER_DIGEST_SIZE];
/** 标识一项 Shader 资产，供 Shader Library 查找。 */
typedef granit_shader_digest granit_shader_content_id;
/** 标识完整 Shader 构建输入，用于缓存失效。 */
typedef granit_shader_digest granit_shader_cache_key;

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

/** Shader Asset 的内存输入；只需提供当前 Renderer 后端对应的 sidecar。 */
typedef struct granit_shader_asset_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const void* manifest_data;
  uint64_t manifest_size;
  const void* sidecar_data;
  uint64_t sidecar_size;
} granit_shader_asset_desc;

#define GRANIT_SHADER_ASSET_DESC_SIZE ((uint32_t)sizeof(granit_shader_asset_desc))
#define GRANIT_SHADER_ASSET_DESC_INIT                                                              \
  {(uint32_t)sizeof(granit_shader_asset_desc), UINT32_C(0), 0, UINT64_C(0), 0, UINT64_C(0)}

/** Shader Asset 中单个后端变体的只读摘要。 */
typedef struct granit_shader_asset_variant_info {
  granit_renderer_backend backend;
  granit_shader_code_format code_format;
  uint32_t profile;
  uint32_t reserved;
  granit_shader_feature_flags required_features;
  uint64_t payload_size;
  granit_shader_digest payload_digest;
} granit_shader_asset_variant_info;

/** 已验证 Shader Asset 清单的只读摘要；entry_point 由调用方提供存储。 */
typedef struct granit_shader_asset_info {
  uint32_t struct_size;
  uint32_t reserved;
  granit_shader_content_id content_id;
  granit_shader_cache_key cache_key;
  granit_shader_stage stage;
  char* entry_point;
  uint32_t entry_point_capacity;
  uint32_t entry_point_length;
  uint32_t variant_count;
  granit_shader_asset_variant_info variants[GRANIT_SHADER_ASSET_MAX_VARIANTS];
} granit_shader_asset_info;

#define GRANIT_SHADER_ASSET_INFO_SIZE ((uint32_t)sizeof(granit_shader_asset_info))
#define GRANIT_SHADER_ASSET_INFO_INIT                                                              \
  {                                                                                                \
    (uint32_t)sizeof(granit_shader_asset_info), UINT32_C(0), {0}, {0}, UINT32_C(0), 0,             \
        UINT32_C(0), UINT32_C(0), UINT32_C(0), {                                                   \
      {                                                                                            \
        UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT64_C(0), UINT64_C(0), {0}          \
      }                                                                                            \
    }                                                                                              \
  }

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
/** 验证 Shader Asset，并按 Renderer 实际能力选择 sidecar 创建 Shader。 */
GRANIT_API granit_result granit_shader_create_from_asset(granit_renderer renderer,
                                                         const granit_shader_asset_desc* desc,
                                                         granit_shader* shader);
/** 校验 Shader Asset 清单并返回元数据；不持有输入或输出内存。 */
GRANIT_API granit_result granit_shader_asset_inspect(const void* manifest_data,
                                                     uint64_t manifest_size,
                                                     granit_shader_asset_info* info);
/** 销毁 Shader 并立即使公开句柄失效。 */
GRANIT_API granit_result granit_shader_destroy(granit_renderer renderer, granit_shader shader);

#ifdef __cplusplus
}
#endif

#endif
