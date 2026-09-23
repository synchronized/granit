// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_BUILDER_H_
#define GRANIT_MATERIAL_BUILDER_H_

#include <stdint.h>

#include <granit/asset_tools/export.h>
#include <granit/core/result.h>

/** Material 构建或检查结果句柄。零值无效。 */
typedef uint64_t granit_asset_tools_material_result;

/** 一个包含逻辑名称表的 `.grshlib`。输入只需在调用期间有效。 */
typedef struct granit_asset_tools_material_shader_library {
  uint32_t struct_size;
  uint32_t reserved;
  const void* archive;
  uint64_t archive_size;
} granit_asset_tools_material_shader_library;

#define GRANIT_ASSET_TOOLS_MATERIAL_SHADER_LIBRARY_INIT                                            \
  {(uint32_t)sizeof(granit_asset_tools_material_shader_library), UINT32_C(0), 0, UINT64_C(0)}

/** Material 源构建描述。所有输入均在调用期间借用。 */
typedef struct granit_asset_tools_material_build_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const char* source_json;
  uint64_t source_json_length;
  const granit_asset_tools_material_shader_library* shader_libraries;
  uint32_t shader_library_count;
  uint32_t reserved2;
} granit_asset_tools_material_build_desc;

#define GRANIT_ASSET_TOOLS_MATERIAL_BUILD_DESC_INIT                                                \
  {(uint32_t)sizeof(granit_asset_tools_material_build_desc),                                       \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 从源 JSON 和 Shader Library 构建确定性 `.grmat`。失败时仍可能返回带诊断的结果句柄。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_material_build(
    const granit_asset_tools_material_build_desc* desc, granit_asset_tools_material_result* result);

/** 检查已有 `.grmat` 字节。失败时仍可能返回带诊断的结果句柄。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_material_inspect(
    const void* archive, uint64_t archive_size, granit_asset_tools_material_result* result);

/** 查询 `.grmat` 字节；视图在结果句柄销毁前有效。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_material_result_get_archive(
    granit_asset_tools_material_result result, const void** data, uint64_t* size);

/** 查询稳定调试 JSON；视图在结果句柄销毁前有效。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_material_result_get_debug_json(
    granit_asset_tools_material_result result, const char** json, uint64_t* length);

/** 查询失败诊断；视图在结果句柄销毁前有效。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_material_result_get_diagnostic(
    granit_asset_tools_material_result result, const char** diagnostic, uint64_t* length);

/** 销毁结果。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_ASSET_TOOLS_API granit_result
granit_asset_tools_material_result_destroy(granit_asset_tools_material_result result);

#ifdef __cplusplus
}
#endif

#endif
