// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASSET_TOOLS_SHADER_LIBRARY_BUILDER_INTERNAL_H_
#define GRANIT_ASSET_TOOLS_SHADER_LIBRARY_BUILDER_INTERNAL_H_

#include <granit/core/result.h>
#include <granit/core/shader_types.h>

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace granit::asset_tools::detail {

granit_result link_shader_library(std::span<const std::filesystem::path> object_paths,
                                  std::span<const std::string> logical_names,
                                  std::string_view library_name,
                                  granit_shader_backend_flags target_backends,
                                  const std::filesystem::path& output_path,
                                  bool& cache_hit) noexcept;

} // namespace granit::asset_tools::detail

#endif
