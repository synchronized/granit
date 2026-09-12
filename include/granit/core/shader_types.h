// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_SHADER_TYPES_H_
#define GRANIT_CORE_SHADER_TYPES_H_

#include <stdint.h>

#include <granit/core/content_id.h>

/** Shader 阶段。 */
typedef uint32_t granit_shader_stage;
#define GRANIT_SHADER_STAGE_VERTEX UINT32_C(1)
#define GRANIT_SHADER_STAGE_FRAGMENT UINT32_C(2)
#define GRANIT_SHADER_STAGE_COMPUTE UINT32_C(3)

/** Renderer 可直接消费的 Shader 代码格式。 */
typedef uint32_t granit_shader_code_format;
#define GRANIT_SHADER_CODE_FORMAT_WGSL UINT32_C(1)
#define GRANIT_SHADER_CODE_FORMAT_SPIRV UINT32_C(2)

/** Shader 构建目标后端位集合。 */
typedef uint32_t granit_shader_backend_flags;
#define GRANIT_SHADER_BACKEND_VULKAN_BIT (UINT32_C(1) << 0)
#define GRANIT_SHADER_BACKEND_WEBGPU_BIT (UINT32_C(1) << 1)
#define GRANIT_SHADER_BACKEND_ALL_BITS                                                             \
  (GRANIT_SHADER_BACKEND_VULKAN_BIT | GRANIT_SHADER_BACKEND_WEBGPU_BIT)

/** Shader 目标能力档位。 */
typedef uint32_t granit_shader_profile;
#define GRANIT_SHADER_PROFILE_PORTABLE UINT32_C(1)

/** 标识一项 Shader 资产，供 Shader Library 查找。 */
typedef granit_asset_content_id granit_shader_content_id;
/** 标识完整 Shader 构建输入，用于缓存失效。 */
typedef granit_content_digest granit_shader_cache_key;

#endif
