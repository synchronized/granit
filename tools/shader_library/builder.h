// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_SHADER_LIBRARY_BUILDER_H_
#define GRANIT_TOOLS_SHADER_LIBRARY_BUILDER_H_

#include <granit/core/result.h>
#include <granit/core/shader_types.h>

#include <filesystem>
#include <span>

namespace granit::tools {

granit_result link_shader_library(std::span<const std::filesystem::path> object_paths,
                                  granit_shader_backend_flags target_backends,
                                  const std::filesystem::path& output_path,
                                  bool& cache_hit) noexcept;

} // namespace granit::tools

#endif
