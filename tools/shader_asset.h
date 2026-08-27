// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_SHADER_ASSET_H_
#define GRANIT_TOOLS_SHADER_ASSET_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace granit::tools {

enum class shader_asset_error {
  success,
  invalid_argument,
  invalid_magic,
  unsupported_schema,
  invalid_layout,
  digest_mismatch,
};

struct shader_asset_source {
  std::string_view wgsl;
  std::span<const std::byte> spirv;
  std::string_view reflection_json;
};

struct shader_asset_view {
  std::string_view wgsl;
  std::span<const std::byte> spirv;
  std::string_view reflection_json;
};

shader_asset_error encode_shader_asset(const shader_asset_source& source,
                                       std::vector<std::byte>& output) noexcept;
shader_asset_error decode_shader_asset(std::span<const std::byte> bytes,
                                       shader_asset_view& output) noexcept;
shader_asset_error store_shader_asset(const std::filesystem::path& path,
                                      std::span<const std::byte> bytes, bool& cache_hit) noexcept;

} // namespace granit::tools

#endif
