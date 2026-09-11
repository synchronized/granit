// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_FORMAT_SHADER_OBJECT_H_
#define GRANIT_SHADER_FORMAT_SHADER_OBJECT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "shader_format/shader_cache_key.h"

#include <granit/core/shader_types.hpp>

namespace granit::detail::shader_format {

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

struct shader_asset_variant {
  shader_asset_backend backend{};
  shader_code_format code_format{};
  shader_profile profile{};
  std::uint64_t required_features = 0;
  std::uint64_t byte_size = 0;
  content_digest digest{};
};

struct shader_asset_source {
  std::string_view wgsl;
  std::span<const std::byte> spirv;
  std::string_view reflection_json;
  shader_cache_key cache_key{};
  granit_shader_backend_flags backend_mask = GRANIT_SHADER_BACKEND_ALL_BITS;
  std::uint64_t required_features = 0;
  shader_stage stage = shader_stage::vertex;
  std::string_view entry_point = "main";
};

struct shader_asset_view {
  std::string_view reflection_json;
  shader_cache_key cache_key{};
  shader_content_id content_id{};
  shader_stage stage{};
  std::string_view entry_point;
  std::array<shader_asset_variant, 2> variants{};
  std::uint32_t variant_count = 0;
};

shader_asset_error encode_shader_asset(const shader_asset_source& source,
                                       std::vector<std::byte>& output) noexcept;
shader_asset_error decode_shader_asset(std::span<const std::byte> bytes,
                                       shader_asset_view& output) noexcept;
const shader_asset_variant* find_shader_asset_variant(const shader_asset_view& asset,
                                                      shader_asset_backend backend,
                                                      shader_profile profile) noexcept;
shader_asset_error validate_shader_asset_payloads(const shader_asset_view& asset,
                                                  std::string_view wgsl,
                                                  std::span<const std::byte> spirv) noexcept;
shader_asset_error validate_shader_asset_payload(const shader_asset_view& asset,
                                                 shader_asset_backend backend,
                                                 std::span<const std::byte> payload) noexcept;
} // namespace granit::detail::shader_format

#endif
