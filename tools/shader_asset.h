// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_SHADER_ASSET_STORAGE_H_
#define GRANIT_TOOLS_SHADER_ASSET_STORAGE_H_

#include "shader_format/shader_object.h"

#include <filesystem>
#include <string>

namespace granit::tools {

std::string shader_file_sha256(const std::filesystem::path& path) noexcept;
granit::detail::shader_format::shader_asset_error
store_shader_asset(const std::filesystem::path& path, std::span<const std::byte> manifest,
                   std::string_view wgsl, std::span<const std::byte> spirv,
                   bool& cache_hit) noexcept;

} // namespace granit::tools

#endif
