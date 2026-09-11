// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_OBJECT_BUILDER_H_
#define GRANIT_SHADER_OBJECT_BUILDER_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/shader_features.h>
#include <granit/core/shader_types.h>
#include <granit/tools/shader_tools_export.h>

/** Shader Object 构建描述。所有路径和字符串只需在调用期间有效。 */
typedef struct granit_shader_tools_object_desc {
  uint32_t struct_size;
  const char* source_path;
  uint64_t source_path_length;
  granit_shader_source_language source_language;
  const char* wgsl_path;
  uint64_t wgsl_path_length;
  const char* spirv_path;
  uint64_t spirv_path_length;
  const char* output_path;
  uint64_t output_path_length;
  const char* tint_revision;
  uint64_t tint_revision_length;
  const char* target_environment;
  uint64_t target_environment_length;
  const char* compile_options;
  uint64_t compile_options_length;
  granit_shader_backend_flags backend_mask;
  granit_shader_feature_flags required_features;
  const char* entry_point;
  uint64_t entry_point_length;
  granit_shader_stage stage;
  uint32_t reserved;
} granit_shader_tools_object_desc;

/** Shader Object 缓存恢复描述。所有路径和字符串只需在调用期间有效。 */
typedef struct granit_shader_tools_object_cache_desc {
  uint32_t struct_size;
  const char* source_path;
  uint64_t source_path_length;
  granit_shader_source_language source_language;
  const char* wgsl_output_path;
  uint64_t wgsl_output_path_length;
  const char* spirv_output_path;
  uint64_t spirv_output_path_length;
  const char* object_path;
  uint64_t object_path_length;
  const char* entry_point;
  uint64_t entry_point_length;
  granit_shader_stage stage;
  const char* tint_revision;
  uint64_t tint_revision_length;
  const char* target_environment;
  uint64_t target_environment_length;
  const char* compile_options;
  uint64_t compile_options_length;
  granit_shader_backend_flags backend_mask;
  granit_shader_feature_flags required_features;
} granit_shader_tools_object_cache_desc;

#ifdef __cplusplus
extern "C" {
#endif

/** 检查已有 SPIR-V/WGSL 并生成 `.grshaderobj`；内容未变化时将 cache_hit 写为 1。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_build_object(const granit_shader_tools_object_desc* desc, uint32_t* cache_hit);

/** 校验 `.grshaderobj` 及其 sidecar，并在命中时恢复编译产物。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_restore_object_cache(
    const granit_shader_tools_object_cache_desc* desc, uint32_t* cache_hit);

#ifdef __cplusplus
}
#endif

#endif
