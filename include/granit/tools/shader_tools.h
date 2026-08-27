// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_TOOLS_H_
#define GRANIT_SHADER_TOOLS_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/tools/shader_tools_export.h>

/** ShaderTools 操作结果句柄。零值无效。 */
typedef uint64_t granit_shader_tools_result;

#define GRANIT_SHADER_TOOLS_STAGE_VERTEX UINT32_C(1)
#define GRANIT_SHADER_TOOLS_STAGE_FRAGMENT UINT32_C(2)
#define GRANIT_SHADER_TOOLS_STAGE_COMPUTE UINT32_C(3)

#define GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER UINT32_C(1)
#define GRANIT_SHADER_TOOLS_BINDING_STORAGE_BUFFER UINT32_C(2)
#define GRANIT_SHADER_TOOLS_BINDING_SAMPLED_TEXTURE UINT32_C(3)
#define GRANIT_SHADER_TOOLS_BINDING_STORAGE_TEXTURE UINT32_C(4)
#define GRANIT_SHADER_TOOLS_BINDING_SAMPLER UINT32_C(5)

#define GRANIT_SHADER_TOOLS_ACCESS_READ UINT32_C(1)
#define GRANIT_SHADER_TOOLS_ACCESS_WRITE UINT32_C(2)
#define GRANIT_SHADER_TOOLS_ACCESS_READ_WRITE UINT32_C(3)

/** WGSL 编译描述。所有字符串均为 UTF-8，调用期间有效且无需以零结尾。 */
typedef struct granit_shader_tools_compile_desc {
  uint32_t struct_size;
  const char* tint_path;
  uint64_t tint_path_length;
  const char* input_path;
  uint64_t input_path_length;
  const char* entry_point;
  uint64_t entry_point_length;
  uint32_t stage;
  const char* output_path;
  uint64_t output_path_length;
} granit_shader_tools_compile_desc;

/** SPIR-V 检查描述。路径为 UTF-8，调用期间有效且无需以零结尾。 */
typedef struct granit_shader_tools_inspect_desc {
  uint32_t struct_size;
  const char* input_path;
  uint64_t input_path_length;
} granit_shader_tools_inspect_desc;

/** 结果摘要。字符串视图在结果句柄销毁前有效，调用者不得释放。 */
typedef struct granit_shader_tools_result_info {
  uint32_t struct_size;
  granit_result status;
  const char* entry_point;
  uint64_t entry_point_length;
  uint32_t stage;
  const char* output;
  uint64_t output_length;
  const char* diagnostic;
  uint64_t diagnostic_length;
} granit_shader_tools_result_info;

/** 单个描述符绑定的后端无关反射记录。名称视图在结果销毁前有效。 */
typedef struct granit_shader_tools_binding_info {
  uint32_t struct_size;
  uint32_t group;
  uint32_t binding;
  uint32_t type;
  uint32_t access;
  const char* name;
  uint64_t name_length;
  uint32_t array_count;
  uint64_t minimum_binding_size;
} granit_shader_tools_binding_info;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 使用 Tint 将 WGSL 编译为 SPIR-V。
 *
 * 只要参数有效就会返回非零结果句柄；即使编译失败也应读取诊断并销毁句柄。
 * 返回值同时写入结果摘要的 status 字段。该函数线程安全。
 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_compile_wgsl(
    const granit_shader_tools_compile_desc* desc, granit_shader_tools_result* result);

/** 检查 SPIR-V 并返回入口点、阶段和反射文本。该函数线程安全。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_inspect_spirv(
    const granit_shader_tools_inspect_desc* desc, granit_shader_tools_result* result);

/** 查询结果。输出结构必须设置 struct_size。该函数线程安全。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_result_get_info(
    granit_shader_tools_result result, granit_shader_tools_result_info* info);

/** 查询结构化描述符绑定数量。编译失败或无绑定时返回零。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_result_get_binding_count(granit_shader_tools_result result, uint64_t* count);

/** 按稳定的 group、binding 数字顺序查询结构化描述符绑定。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_result_get_binding(
    granit_shader_tools_result result, uint64_t index, granit_shader_tools_binding_info* binding);

/** 销毁结果句柄。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_result_destroy(granit_shader_tools_result result);

#ifdef __cplusplus
}
#endif

#endif
