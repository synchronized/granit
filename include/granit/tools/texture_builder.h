// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEXTURE_BUILDER_H_
#define GRANIT_TEXTURE_BUILDER_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/renderer/texture_asset.h>
#include <granit/tools/asset_tools_export.h>

/** Texture Asset 构建或检查结果句柄。零值无效。 */
typedef uint64_t granit_asset_tools_texture_result;

/** 一个 GPU 格式变体及其完整子资源布局。所有输入只需在构建调用期间有效。 */
typedef struct granit_asset_tools_texture_variant_desc {
  uint32_t struct_size;
  granit_texture_format format;
  granit_texture_usage usage;
  uint32_t reserved;
  const void* payload;
  uint64_t payload_size;
  const granit_texture_asset_subresource_info* subresources;
  uint32_t subresource_count;
  uint32_t reserved2;
} granit_asset_tools_texture_variant_desc;

#define GRANIT_ASSET_TOOLS_TEXTURE_VARIANT_DESC_INIT                                               \
  {(uint32_t)sizeof(granit_asset_tools_texture_variant_desc),                                      \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0)}

/**
 * Texture Asset 构建描述。内容 ID、负载偏移、大小和摘要由 Builder 生成；变体顺序即运行时
 * 选择优先级。
 */
typedef struct granit_asset_tools_texture_build_desc {
  uint32_t struct_size;
  granit_texture_dimension dimension;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t array_layers;
  uint32_t mip_levels;
  uint32_t variant_count;
  const granit_asset_tools_texture_variant_desc* variants;
  uint32_t reserved[2];
} granit_asset_tools_texture_build_desc;

#define GRANIT_ASSET_TOOLS_TEXTURE_BUILD_DESC_INIT                                                 \
  {                                                                                                \
    (uint32_t)sizeof(granit_asset_tools_texture_build_desc), GRANIT_TEXTURE_DIMENSION_2D,          \
        UINT32_C(1), UINT32_C(1), UINT32_C(1), UINT32_C(1), UINT32_C(1), UINT32_C(0), 0, {         \
      UINT32_C(0), UINT32_C(0)                                                                     \
    }                                                                                              \
  }

#ifdef __cplusplus
extern "C" {
#endif

/** 构建确定性 Texture Asset Manifest 与按变体顺序拼接的负载。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_texture_build(
    const granit_asset_tools_texture_build_desc* desc, granit_asset_tools_texture_result* result);

/** 严格检查已有 Texture Asset Manifest，并生成稳定调试 JSON。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_texture_inspect(
    const void* manifest, uint64_t manifest_size, granit_asset_tools_texture_result* result);

/** 查询 Manifest 字节；视图在结果句柄销毁前有效。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_texture_result_get_manifest(
    granit_asset_tools_texture_result result, const void** data, uint64_t* size);

/** 查询拼接负载；检查结果返回空视图。视图在结果句柄销毁前有效。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_texture_result_get_payload(
    granit_asset_tools_texture_result result, const void** data, uint64_t* size);

/** 查询稳定调试 JSON；视图在结果句柄销毁前有效。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_texture_result_get_debug_json(
    granit_asset_tools_texture_result result, const char** json, uint64_t* length);

/** 查询失败诊断；视图在结果句柄销毁前有效。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_texture_result_get_diagnostic(
    granit_asset_tools_texture_result result, const char** diagnostic, uint64_t* length);

/** 销毁结果。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_ASSET_TOOLS_API granit_result
granit_asset_tools_texture_result_destroy(granit_asset_tools_texture_result result);

#ifdef __cplusplus
}
#endif

#endif
