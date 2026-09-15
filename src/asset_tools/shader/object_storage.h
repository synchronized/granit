// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASSET_TOOLS_SHADER_OBJECT_STORAGE_H_
#define GRANIT_ASSET_TOOLS_SHADER_OBJECT_STORAGE_H_

#include "asset_formats/shader/shader_object.h"

#include <filesystem>
#include <string>

namespace granit::asset_tools::detail {

std::string file_sha256_hex(const std::filesystem::path& path) noexcept;
granit::detail::shader_format::shader_object_error
store_shader_object(const std::filesystem::path& path, std::span<const std::byte> manifest,
                    std::string_view wgsl, std::span<const std::byte> spirv,
                    bool& cache_hit) noexcept;

} // namespace granit::asset_tools::detail

#endif
