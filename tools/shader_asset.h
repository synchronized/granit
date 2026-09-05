// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_SHADER_ASSET_H_
#define GRANIT_TOOLS_SHADER_ASSET_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace granit::tools {

using shader_cache_key = std::array<std::byte, 32>;

struct shader_cache_context {
  std::string_view wgsl;
  std::string_view entry_point;
  std::string_view stage;
  std::string_view tint_revision;
  std::string_view target_environment;
  std::string_view compile_options;
};

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
  shader_cache_key cache_key{};
};

struct shader_asset_view {
  std::string_view reflection_json;
  shader_cache_key cache_key{};
  shader_cache_key wgsl_digest{};
  shader_cache_key spirv_digest{};
  std::uint64_t wgsl_size = 0;
  std::uint64_t spirv_size = 0;
};

shader_cache_key make_shader_cache_key(const shader_cache_context& context) noexcept;
shader_asset_error encode_shader_asset(const shader_asset_source& source,
                                       std::vector<std::byte>& output) noexcept;
shader_asset_error decode_shader_asset(std::span<const std::byte> bytes,
                                       shader_asset_view& output) noexcept;
shader_asset_error validate_shader_asset_payloads(const shader_asset_view& asset,
                                                  std::string_view wgsl,
                                                  std::span<const std::byte> spirv) noexcept;
shader_asset_error store_shader_asset(const std::filesystem::path& path,
                                      std::span<const std::byte> manifest, std::string_view wgsl,
                                      std::span<const std::byte> spirv, bool& cache_hit) noexcept;

} // namespace granit::tools

#endif
