// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_TOOLS_H_
#define GRANIT_SHADER_TOOLS_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/shader_features.h>
#include <granit/tools/shader_tools_export.h>

/** ShaderTools 操作结果句柄。零值无效。 */
typedef uint64_t granit_shader_tools_result;

/** 用于核对 WGSL 与最终 SPIR-V 的预期 Binding 键。 */
typedef struct granit_shader_tools_expected_binding {
  uint32_t struct_size;
  uint32_t group;
  uint32_t binding;
} granit_shader_tools_expected_binding;

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

#define GRANIT_SHADER_TOOLS_SCALAR_FLOAT UINT32_C(1)
#define GRANIT_SHADER_TOOLS_SCALAR_SINT UINT32_C(2)
#define GRANIT_SHADER_TOOLS_SCALAR_UINT UINT32_C(3)

#define GRANIT_SHADER_TOOLS_ASSET_BACKEND_VULKAN UINT32_C(1)
#define GRANIT_SHADER_TOOLS_ASSET_BACKEND_WEBGPU UINT32_C(2)
#define GRANIT_SHADER_TOOLS_ASSET_BACKEND_ALL UINT32_C(3)

/** ShaderTools 内置目标档位的静态能力；与构建机 GPU 无关。 */
typedef struct granit_shader_tools_target_capabilities {
  uint32_t struct_size;
  uint32_t backend;
  uint32_t profile;
  uint32_t reserved;
  granit_shader_feature_flags supported_features;
} granit_shader_tools_target_capabilities;

#define GRANIT_SHADER_TOOLS_TARGET_CAPABILITIES_INIT                                               \
  {(uint32_t)sizeof(granit_shader_tools_target_capabilities), UINT32_C(0),                         \
   GRANIT_SHADER_PROFILE_PORTABLE, UINT32_C(0), UINT64_C(0)}

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
  uint32_t validate_binding_set;
  const granit_shader_tools_expected_binding* expected_bindings;
  uint64_t expected_binding_count;
} granit_shader_tools_compile_desc;

/** SPIR-V 检查描述。路径为 UTF-8，调用期间有效且无需以零结尾。 */
typedef struct granit_shader_tools_inspect_desc {
  uint32_t struct_size;
  const char* input_path;
  uint64_t input_path_length;
  uint32_t validate_binding_set;
  const granit_shader_tools_expected_binding* expected_bindings;
  uint64_t expected_binding_count;
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

/** Shader 资产写入描述。路径和字符串均为 UTF-8，调用期间有效且无需以零结尾。 */
typedef struct granit_shader_tools_asset_desc {
  uint32_t struct_size;
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
  /** 要写入清单的后端位集合；必须是 GRANIT_SHADER_TOOLS_ASSET_BACKEND_* 的非零组合。 */
  uint32_t backend_mask;
} granit_shader_tools_asset_desc;

/** Shader 资产缓存恢复描述。所有路径和字符串均在调用期间有效。 */
typedef struct granit_shader_tools_cache_desc {
  uint32_t struct_size;
  const char* wgsl_path;
  uint64_t wgsl_path_length;
  const char* spirv_output_path;
  uint64_t spirv_output_path_length;
  const char* asset_path;
  uint64_t asset_path_length;
  const char* entry_point;
  uint64_t entry_point_length;
  uint32_t stage;
  const char* tint_revision;
  uint64_t tint_revision_length;
  const char* target_environment;
  uint64_t target_environment_length;
  const char* compile_options;
  uint64_t compile_options_length;
  /** 期望资产包含的精确后端位集合；语义与 granit_shader_tools_asset_desc 相同。 */
  uint32_t backend_mask;
} granit_shader_tools_cache_desc;

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

/** Vertex 输入或 Fragment 输出的接口变量。名称视图在结果销毁前有效。 */
typedef struct granit_shader_tools_interface_variable_info {
  uint32_t struct_size;
  uint32_t location;
  uint32_t component;
  uint32_t scalar_type;
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
  uint32_t scalar_type;
  uint32_t bit_width;
  const char* name;
  uint64_t name_length;
  uint64_t default_value;
  uint32_t default_value_size;
} granit_shader_tools_override_info;

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

/** 查询 Vertex 输入数量。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_result_get_vertex_input_count(
    granit_shader_tools_result result, uint64_t* count);

/** 按 Location、Component 顺序查询 Vertex 输入。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_result_get_vertex_input(granit_shader_tools_result result, uint64_t index,
                                            granit_shader_tools_interface_variable_info* input);

/** 查询 Fragment 输出数量。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_result_get_fragment_output_count(
    granit_shader_tools_result result, uint64_t* count);

/** 按 Location、Component 顺序查询 Fragment 输出。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_result_get_fragment_output(granit_shader_tools_result result, uint64_t index,
                                               granit_shader_tools_interface_variable_info* output);

/** 查询 Compute Workgroup 大小。非 Compute 阶段返回零值。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_result_get_workgroup_size(
    granit_shader_tools_result result, granit_shader_tools_workgroup_size* size);

/** 查询按常量 ID 排序的 Override 数量。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_result_get_override_count(granit_shader_tools_result result, uint64_t* count);

/** 按常量 ID 顺序查询 Override。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_result_get_override(granit_shader_tools_result result, uint64_t index,
                                        granit_shader_tools_override_info* override_info);

/** 查询稳定排序的 UTF-8 反射 JSON。视图在结果销毁前有效。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_result_get_reflection_json(
    granit_shader_tools_result result, const char** json, uint64_t* length);

/**
 * 将稳定反射清单写入 output_path，并将 WGSL、SPIR-V 分别写入同名 .wgsl、.spv sidecar。
 * cache_hit 仅在清单和两个 sidecar 均逐字节相同时写为 1，否则写为 0。
 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_result_write_asset(
    granit_shader_tools_result result, const granit_shader_tools_asset_desc* desc,
    uint32_t* cache_hit);

/**
 * 在运行 Tint 前尝试恢复确定性 Shader 资产。
 *
 * 有效缓存命中时校验清单和两个 sidecar，将 SPIR-V 写入 spirv_output_path，并把 cache_hit
 * 写为 1；任一文件不存在、损坏或缓存键不匹配均作为正常未命中返回 GRANIT_SUCCESS 和 0。
 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_restore_asset_cache(
    const granit_shader_tools_cache_desc* desc, uint32_t* cache_hit);

/** 查询内置目标档位支持的静态特性；当前 backend 使用 ASSET_BACKEND 单值。 */
GRANIT_SHADER_TOOLS_API granit_result granit_shader_tools_get_target_capabilities(
    uint32_t backend, uint32_t profile, granit_shader_tools_target_capabilities* capabilities);

/** 销毁结果句柄。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_SHADER_TOOLS_API granit_result
granit_shader_tools_result_destroy(granit_shader_tools_result result);

#ifdef __cplusplus
}
#endif

#endif
