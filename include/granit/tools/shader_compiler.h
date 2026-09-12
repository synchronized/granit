// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_COMPILER_H_
#define GRANIT_SHADER_COMPILER_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/shader_features.h>
#include <granit/core/shader_types.h>
#include <granit/tools/shader_reflection.h>
#include <granit/tools/shader_tools_export.h>

/** Shader 编译结果句柄。零值无效。 */
typedef uint64_t granit_shader_tools_compilation;
/** Shader Compiler 句柄。零值无效。 */
typedef uint64_t granit_shader_tools_compiler;

/** ShaderTools 内置目标档位的静态能力；与构建机 GPU 无关。 */
typedef struct granit_shader_tools_target_capabilities {
  uint32_t struct_size;
  granit_shader_backend_flags backend;
  granit_shader_profile profile;
  uint32_t reserved;
  granit_shader_feature_flags supported_features;
} granit_shader_tools_target_capabilities;

#define GRANIT_SHADER_TOOLS_TARGET_CAPABILITIES_INIT                                               \
  {(uint32_t)sizeof(granit_shader_tools_target_capabilities), UINT32_C(0),                         \
   GRANIT_SHADER_PROFILE_PORTABLE, UINT32_C(0), UINT64_C(0)}

/** Shader Compiler 配置。所有路径均在创建调用期间借用。 */
typedef struct granit_shader_tools_compiler_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const char* dxc_path;
  uint64_t dxc_path_length;
  const char* tint_path;
  uint64_t tint_path_length;
} granit_shader_tools_compiler_desc;

#define GRANIT_SHADER_TOOLS_COMPILER_DESC_INIT                                                     \
  {(uint32_t)sizeof(granit_shader_tools_compiler_desc), UINT32_C(0), 0, UINT64_C(0), 0, UINT64_C(0)}

typedef struct granit_shader_tools_define {
  uint32_t struct_size;
  uint32_t reserved;
  const char* name;
  uint64_t name_length;
  const char* value;
  uint64_t value_length;
} granit_shader_tools_define;

#define GRANIT_SHADER_TOOLS_DEFINE_INIT                                                            \
  {(uint32_t)sizeof(granit_shader_tools_define), UINT32_C(0), 0, UINT64_C(0), 0, UINT64_C(0)}

/** HLSL 编译描述；spirv_output_path 与 wgsl_output_path 均必填。 */
typedef struct granit_shader_tools_compile_desc {
  uint32_t struct_size;
  granit_shader_stage stage;
  /** 产物面向的后端非零位集合；当前编译路径会生成后续打包所需的全部中间载荷。 */
  granit_shader_backend_flags target_backends;
  const char* input_path;
  uint64_t input_path_length;
  const char* entry_point;
  uint64_t entry_point_length;
  const char* spirv_output_path;
  uint64_t spirv_output_path_length;
  const char* wgsl_output_path;
  uint64_t wgsl_output_path_length;
  /** 实现按名称排序并拒绝重复项。 */
  const granit_shader_tools_define* defines;
  uint32_t define_count;
  uint32_t validate_binding_set;
  const granit_shader_tools_expected_binding* expected_bindings;
  uint64_t expected_binding_count;
} granit_shader_tools_compile_desc;

#define GRANIT_SHADER_TOOLS_COMPILE_DESC_INIT                                                      \
  {(uint32_t)sizeof(granit_shader_tools_compile_desc),                                             \
   GRANIT_SHADER_STAGE_VERTEX,                                                                     \
   GRANIT_SHADER_BACKEND_ALL_BITS,                                                                 \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0)}

/** 编译摘要。字符串视图在 Compilation 销毁前有效，调用者不得释放。 */
typedef struct granit_shader_tools_compilation_info {
  uint32_t struct_size;
  granit_result status;
  const char* entry_point;
  uint64_t entry_point_length;
  granit_shader_stage stage;
  const char* output;
  uint64_t output_length;
  const char* diagnostic;
  uint64_t diagnostic_length;
} granit_shader_tools_compilation_info;

#ifdef __cplusplus
extern "C" {
#endif

/** 创建 Compiler；工具路径会复制到句柄中，创建返回后调用方可释放输入字符串。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_compiler_create(
    const granit_shader_tools_compiler_desc* desc, granit_shader_tools_compiler* compiler);

/**
 * 使用 Compiler 编译 HLSL。只要编译已启动便返回 Compilation，诊断由其持有。
 * Compiler 可由多个线程并发调用，描述中的字符串和数组只需在调用期间有效。
 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_compiler_compile(
    granit_shader_tools_compiler compiler, const granit_shader_tools_compile_desc* desc,
    granit_shader_tools_compilation* compilation);

/** 销毁 Compiler。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_compiler_destroy(granit_shader_tools_compiler compiler);

/** 查询结果。输出结构必须设置 struct_size。该函数线程安全。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_compilation_get_info(
    granit_shader_tools_compilation compilation, granit_shader_tools_compilation_info* info);

/** 获取编译结果对应的独立反射句柄；该句柄可晚于编译结果销毁。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_compilation_get_reflection(
    granit_shader_tools_compilation compilation, granit_shader_tools_reflection* reflection);

/** 查询编译生成的 SPIR-V；视图在编译结果销毁前有效。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_compilation_get_spirv(
    granit_shader_tools_compilation compilation, const void** data, uint64_t* size);

/** 查询编译生成或规范化的 WGSL；视图在编译结果销毁前有效。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_compilation_get_wgsl(
    granit_shader_tools_compilation compilation, const char** source, uint64_t* length);

/** 查询内置目标档位支持的静态特性；当前 backend 使用 ASSET_BACKEND 单值。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_get_target_capabilities(
    granit_shader_backend_flags backend, granit_shader_profile profile,
    granit_shader_tools_target_capabilities* capabilities);

/** 销毁 Compilation。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_compilation_destroy(granit_shader_tools_compilation compilation);

#ifdef __cplusplus
}
#endif

#endif
