// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_REFLECTION_H_
#define GRANIT_SHADER_REFLECTION_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/shader_features.h>
#include <granit/core/shader_types.h>
#include <granit/tools/shader_tools_export.h>

/** Shader 反射结果句柄。零值无效。 */
typedef uint64_t granit_shader_tools_reflection;
/** 用于核对 WGSL 与最终 SPIR-V 的预期 Binding 键。 */
typedef struct granit_shader_tools_expected_binding {
  uint32_t struct_size;
  uint32_t group;
  uint32_t binding;
} granit_shader_tools_expected_binding;

typedef uint32_t granit_shader_tools_binding_type;
#define GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER UINT32_C(1)
#define GRANIT_SHADER_TOOLS_BINDING_STORAGE_BUFFER UINT32_C(2)
#define GRANIT_SHADER_TOOLS_BINDING_SAMPLED_TEXTURE UINT32_C(3)
#define GRANIT_SHADER_TOOLS_BINDING_STORAGE_TEXTURE UINT32_C(4)
#define GRANIT_SHADER_TOOLS_BINDING_SAMPLER UINT32_C(5)

typedef uint32_t granit_shader_tools_binding_access;
#define GRANIT_SHADER_TOOLS_ACCESS_READ UINT32_C(1)
#define GRANIT_SHADER_TOOLS_ACCESS_WRITE UINT32_C(2)
#define GRANIT_SHADER_TOOLS_ACCESS_READ_WRITE UINT32_C(3)

typedef uint32_t granit_shader_tools_scalar_type;
#define GRANIT_SHADER_TOOLS_SCALAR_FLOAT UINT32_C(1)
#define GRANIT_SHADER_TOOLS_SCALAR_SINT UINT32_C(2)
#define GRANIT_SHADER_TOOLS_SCALAR_UINT UINT32_C(3)

/** SPIR-V 检查描述。路径为 UTF-8，调用期间有效且无需以零结尾。 */
typedef struct granit_shader_tools_inspect_desc {
  uint32_t struct_size;
  const char* input_path;
  uint64_t input_path_length;
  uint32_t validate_binding_set;
  const granit_shader_tools_expected_binding* expected_bindings;
  uint64_t expected_binding_count;
} granit_shader_tools_inspect_desc;

/** 反射摘要。字符串视图在反射句柄销毁前有效，调用者不得释放。 */
typedef struct granit_shader_tools_reflection_info {
  uint32_t struct_size;
  granit_result status;
  const char* entry_point;
  uint64_t entry_point_length;
  granit_shader_stage stage;
  const char* output;
  uint64_t output_length;
  const char* diagnostic;
  uint64_t diagnostic_length;
} granit_shader_tools_reflection_info;

/** Shader 资产写入描述。路径和字符串均为 UTF-8，调用期间有效且无需以零结尾。 */
typedef struct granit_shader_tools_asset_desc {
  uint32_t struct_size;
  /** 用于缓存身份的原始源码；不作为 sidecar 写入。 */
  const char* source_path;
  uint64_t source_path_length;
  granit_shader_source_language source_language;
  /** 已生成的 WebGPU WGSL 载荷。 */
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
  /** 要写入清单的后端位集合；必须是 GRANIT_SHADER_BACKEND_*_BIT 的非零组合。 */
  granit_shader_backend_flags backend_mask;
  /** 所有导出变体必须支持的 GRANIT_SHADER_FEATURE_* 位集合。 */
  granit_shader_feature_flags required_features;
} granit_shader_tools_asset_desc;

/** 单个描述符绑定的后端无关反射记录。名称视图在反射销毁前有效。 */
typedef struct granit_shader_tools_binding_info {
  uint32_t struct_size;
  uint32_t group;
  uint32_t binding;
  granit_shader_tools_binding_type type;
  granit_shader_tools_binding_access access;
  const char* name;
  uint64_t name_length;
  uint32_t array_count;
  uint64_t minimum_binding_size;
} granit_shader_tools_binding_info;

/** Vertex 输入或 Fragment 输出的接口变量。名称视图在反射销毁前有效。 */
typedef struct granit_shader_tools_interface_variable_info {
  uint32_t struct_size;
  uint32_t location;
  uint32_t component;
  granit_shader_tools_scalar_type scalar_type;
  uint32_t bit_width;
  uint32_t vector_size;
  const char* name;
  uint64_t name_length;
} granit_shader_tools_interface_variable_info;

/** Compute 入口点的固定 Workgroup 大小。非 Compute 阶段返回零值。 */
typedef struct granit_shader_tools_workgroup_size {
  uint32_t struct_size;
  uint32_t x;
  uint32_t y;
  uint32_t z;
} granit_shader_tools_workgroup_size;

/** Override／Specialization Constant 记录。默认值保存为原始小端位模式。 */
typedef struct granit_shader_tools_override_info {
  uint32_t struct_size;
  uint32_t id;
  granit_shader_tools_scalar_type scalar_type;
  uint32_t bit_width;
  const char* name;
  uint64_t name_length;
  uint64_t default_value;
  uint32_t default_value_size;
} granit_shader_tools_override_info;

#ifdef __cplusplus
extern "C" {
#endif

/** 检查 SPIR-V 并返回入口点、阶段和反射文本。该函数线程安全。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_inspect_spirv(
    const granit_shader_tools_inspect_desc* desc, granit_shader_tools_reflection* reflection);

/** 查询反射摘要。输出结构必须设置 struct_size。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_info(
    granit_shader_tools_reflection reflection, granit_shader_tools_reflection_info* info);

/** 查询结构化描述符绑定数量。编译失败或无绑定时返回零。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_binding_count(
    granit_shader_tools_reflection reflection, uint64_t* count);

/** 按稳定的 group、binding 数字顺序查询结构化描述符绑定。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_binding(
    granit_shader_tools_reflection reflection, uint64_t index,
    granit_shader_tools_binding_info* binding);

/** 查询 Vertex 输入数量。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_vertex_input_count(
    granit_shader_tools_reflection reflection, uint64_t* count);

/** 按 Location、Component 顺序查询 Vertex 输入。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_vertex_input(
    granit_shader_tools_reflection reflection, uint64_t index,
    granit_shader_tools_interface_variable_info* input);

/** 查询 Fragment 输出数量。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_fragment_output_count(
    granit_shader_tools_reflection reflection, uint64_t* count);

/** 按 Location、Component 顺序查询 Fragment 输出。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_fragment_output(
    granit_shader_tools_reflection reflection, uint64_t index,
    granit_shader_tools_interface_variable_info* output);

/** 查询 Compute Workgroup 大小。非 Compute 阶段返回零值。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_workgroup_size(
    granit_shader_tools_reflection reflection, granit_shader_tools_workgroup_size* size);

/** 查询按常量 ID 排序的 Override 数量。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_override_count(
    granit_shader_tools_reflection reflection, uint64_t* count);

/** 按常量 ID 顺序查询 Override。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_override(
    granit_shader_tools_reflection reflection, uint64_t index,
    granit_shader_tools_override_info* override_info);

/** 查询稳定排序的 UTF-8 反射 JSON。视图在反射销毁前有效。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_get_json(
    granit_shader_tools_reflection reflection, const char** json, uint64_t* length);

/** 使用已检查的反射和描述中的载荷写入 Shader 资产。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_reflection_write_asset(
    granit_shader_tools_reflection reflection, const granit_shader_tools_asset_desc* desc,
    uint32_t* cache_hit);

/** 销毁反射句柄。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_reflection_destroy(granit_shader_tools_reflection reflection);

#ifdef __cplusplus
}
#endif

#endif
