// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_SHADER_TYPES_HPP_
#define GRANIT_CORE_SHADER_TYPES_HPP_

#include <cstdint>

#include <granit/core/content_id.hpp>
#include <granit/core/shader_types.h>

namespace granit {

enum class shader_stage : std::uint32_t {
  vertex = GRANIT_SHADER_STAGE_VERTEX,
  fragment = GRANIT_SHADER_STAGE_FRAGMENT,
  compute = GRANIT_SHADER_STAGE_COMPUTE,
};

enum class shader_code_format : std::uint32_t {
  wgsl = GRANIT_SHADER_CODE_FORMAT_WGSL,
  spirv = GRANIT_SHADER_CODE_FORMAT_SPIRV,
};

enum class shader_backend : std::uint32_t {
  none = 0,
  vulkan = GRANIT_SHADER_BACKEND_VULKAN_BIT,
  webgpu = GRANIT_SHADER_BACKEND_WEBGPU_BIT,
  all = GRANIT_SHADER_BACKEND_ALL_BITS,
};

enum class shader_profile : std::uint32_t {
  portable = GRANIT_SHADER_PROFILE_PORTABLE,
};

/** 标识一项 Shader 资产，供 Shader Library 查找。 */
using shader_content_id = asset_content_id;
/** 标识完整 Shader 构建输入，用于缓存失效。 */
using shader_cache_key = content_digest;

} // namespace granit

#endif
