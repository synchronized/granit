// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_SHADER_ASSET_STORAGE_H_
#define GRANIT_TOOLS_SHADER_ASSET_STORAGE_H_

#include "shader_format/shader_object.h"

#include <filesystem>
#include <string>

namespace granit::tools {

std::string file_sha256_hex(const std::filesystem::path& path) noexcept;
granit::detail::shader_format::shader_object_error
store_shader_object(const std::filesystem::path& path, std::span<const std::byte> manifest,
                    std::string_view wgsl, std::span<const std::byte> spirv,
                    bool& cache_hit) noexcept;

} // namespace granit::tools

#endif
