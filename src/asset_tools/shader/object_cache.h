// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASSET_TOOLS_SHADER_OBJECT_CACHE_H_
#define GRANIT_ASSET_TOOLS_SHADER_OBJECT_CACHE_H_

#include <granit/core/result.h>
#include <granit/core/shader_features.h>
#include <granit/core/shader_types.hpp>

#include <filesystem>
#include <string_view>

namespace granit::asset_tools::detail {

/** Library Builder 私有的单 Shader 缓存上下文。 */
struct shader_object_cache_context {
  std::filesystem::path source_path;
  std::filesystem::path wgsl_path;
  std::filesystem::path spirv_path;
  std::filesystem::path object_path;
  std::string_view entry_point;
  shader_stage stage{shader_stage::vertex};
  std::string_view tool_identity;
  std::string_view target_environment;
  std::string_view compile_options;
  granit_shader_backend_flags backend_mask{GRANIT_SHADER_BACKEND_ALL_BITS};
  granit_shader_feature_flags required_features{};
};

granit_result restore_shader_object_cache(const shader_object_cache_context& context,
                                          bool& cache_hit) noexcept;
granit_result write_shader_object_cache(const shader_object_cache_context& context,
                                        bool& cache_hit) noexcept;

} // namespace granit::asset_tools::detail

#endif
