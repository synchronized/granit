// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_LIBRARY_BUILDER_H_
#define GRANIT_SHADER_LIBRARY_BUILDER_H_

#include <stdint.h>

#include <granit/asset_tools/export.h>
#include <granit/core/result.h>
#include <granit/core/shader_types.h>

/** Shader Library 源构建结果句柄。零值无效。 */
typedef uint64_t granit_asset_tools_shader_library_result;

/** Shader Library 源构建结果信息；字符串视图在结果句柄销毁前有效。 */
typedef struct granit_asset_tools_shader_library_result_info {
  uint32_t struct_size;
  uint32_t reserved;
  uint32_t cache_hit;
  uint32_t reserved2;
  const char* failed_shader;
  uint64_t failed_shader_length;
  const char* diagnostic;
  uint64_t diagnostic_length;
} granit_asset_tools_shader_library_result_info;

#define GRANIT_ASSET_TOOLS_SHADER_LIBRARY_RESULT_INFO_INIT                                         \
  {(uint32_t)sizeof(granit_asset_tools_shader_library_result_info),                                \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0)}

/** HLSL-first Shader Library 源构建描述。所有路径只需在调用期间有效。 */
typedef struct granit_asset_tools_shader_source_library_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const char* manifest_path;
  uint64_t manifest_path_length;
  const char* toolchain_root;
  uint64_t toolchain_root_length;
  const char* cache_path;
  uint64_t cache_path_length;
  const char* output_path;
  uint64_t output_path_length;
} granit_asset_tools_shader_source_library_desc;

#define GRANIT_ASSET_TOOLS_SHADER_SOURCE_LIBRARY_DESC_INIT                                         \
  {(uint32_t)sizeof(granit_asset_tools_shader_source_library_desc),                                \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 从 `.grshlib.json` 一次生成包含逻辑名称表的跨后端 Library 和私有 Object 缓存。
 * 描述中的 UTF-8 路径仅在调用期间借用；清单或路径无效返回 INVALID_ARGUMENT，缺少工具返回
 * NOT_READY，编译或写入失败返回 INITIALIZATION_FAILED。参数结构无效时结果保持零值；其他失败仍
 * 可能返回带诊断的结果句柄。不同输出与缓存路径可由多个线程并发构建。
 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_shader_build_library_from_manifest(
    const granit_asset_tools_shader_source_library_desc* desc,
    granit_asset_tools_shader_library_result* result);

/** 查询缓存状态、失败 Shader 逻辑名称和诊断；字符串视图在结果句柄销毁前有效。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_shader_library_result_get_info(
    granit_asset_tools_shader_library_result result,
    granit_asset_tools_shader_library_result_info* info);

/** 销毁结果。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_ASSET_TOOLS_API granit_result
granit_asset_tools_shader_library_result_destroy(granit_asset_tools_shader_library_result result);

#ifdef __cplusplus
}
#endif

#endif
