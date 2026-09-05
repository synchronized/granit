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
  std::string_view source;
  std::string_view source_language;
  std::string_view entry_point;
  std::string_view stage;
  std::string_view tint_revision;
  std::string_view target_environment;
  std::string_view compile_options;
  std::uint64_t required_features = 0;
};

enum class shader_asset_error {
  success,
  invalid_argument,
  invalid_magic,
  unsupported_schema,
  invalid_layout,
  digest_mismatch,
};

enum class shader_asset_backend : std::uint32_t {
  webgpu = 1,
  vulkan = 2,
};

enum class shader_asset_code_format : std::uint32_t {
  wgsl = 1,
  spirv = 2,
};

enum class shader_asset_profile : std::uint32_t {
  portable = 1,
};

struct shader_asset_variant {
  shader_asset_backend backend{};
  shader_asset_code_format code_format{};
  shader_asset_profile profile{};
  std::uint64_t required_features = 0;
  std::uint64_t byte_size = 0;
  shader_cache_key digest{};
};

struct shader_asset_source {
  std::string_view wgsl;
  std::span<const std::byte> spirv;
  std::string_view reflection_json;
  shader_cache_key cache_key{};
  std::uint32_t backend_mask = 3;
  std::uint64_t required_features = 0;
};

struct shader_asset_view {
  std::string_view reflection_json;
  shader_cache_key cache_key{};
  std::array<shader_asset_variant, 2> variants{};
  std::uint32_t variant_count = 0;
};

shader_cache_key make_shader_cache_key(const shader_cache_context& context) noexcept;
shader_asset_error encode_shader_asset(const shader_asset_source& source,
                                       std::vector<std::byte>& output) noexcept;
shader_asset_error decode_shader_asset(std::span<const std::byte> bytes,
                                       shader_asset_view& output) noexcept;
const shader_asset_variant* find_shader_asset_variant(const shader_asset_view& asset,
                                                      shader_asset_backend backend,
                                                      shader_asset_profile profile) noexcept;
shader_asset_error validate_shader_asset_payloads(const shader_asset_view& asset,
                                                  std::string_view wgsl,
                                                  std::span<const std::byte> spirv) noexcept;
shader_asset_error store_shader_asset(const std::filesystem::path& path,
                                      std::span<const std::byte> manifest, std::string_view wgsl,
                                      std::span<const std::byte> spirv, bool& cache_hit) noexcept;

} // namespace granit::tools

#endif
