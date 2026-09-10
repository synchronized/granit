// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASSETS_SHADER_LIBRARY_H_
#define GRANIT_ASSETS_SHADER_LIBRARY_H_

#include "assets/shader_asset.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace granit::tools {

inline constexpr std::uint32_t shader_library_backend_vulkan = UINT32_C(1) << 0;
inline constexpr std::uint32_t shader_library_backend_webgpu = UINT32_C(1) << 1;
inline constexpr std::uint32_t shader_library_backend_all =
    shader_library_backend_vulkan | shader_library_backend_webgpu;

enum class shader_library_error {
  success,
  invalid_argument,
  invalid_magic,
  unsupported_schema,
  invalid_layout,
  digest_mismatch,
  invalid_shader_asset,
  missing_payload,
  conflicting_shader,
  conflicting_payload,
};

struct shader_library_asset_source {
  std::span<const std::byte> manifest;
  std::span<const std::byte> wgsl;
  std::span<const std::byte> spirv;
};

struct shader_library_encode_desc {
  std::span<const shader_library_asset_source> assets;
  std::uint32_t backend_mask = shader_library_backend_all;
};

struct shader_library_variant {
  shader_asset_backend backend{};
  shader_asset_code_format code_format{};
  shader_asset_profile profile{};
  std::uint64_t required_features = 0;
  std::uint32_t payload_index = 0;
  shader_cache_key payload_digest{};

  friend bool operator==(const shader_library_variant&, const shader_library_variant&) = default;
};

struct shader_library_shader {
  shader_cache_key content_id{};
  shader_cache_key cache_key{};
  std::uint32_t stage = 0;
  std::string_view entry_point;
  std::string_view reflection_json;
  std::vector<shader_library_variant> variants;
};

struct shader_library_payload {
  shader_cache_key digest{};
  std::span<const std::byte> bytes;
};

struct shader_library_view {
  shader_cache_key content_digest{};
  std::uint32_t backend_mask = 0;
  std::vector<shader_library_shader> shaders;
  std::vector<shader_library_payload> payloads;
};

[[nodiscard]] shader_library_error encode_shader_library(const shader_library_encode_desc& desc,
                                                         std::vector<std::byte>& output) noexcept;
[[nodiscard]] shader_library_error decode_shader_library(std::span<const std::byte> bytes,
                                                         shader_library_view& output) noexcept;
[[nodiscard]] const shader_library_shader*
find_shader_library_shader(const shader_library_view& library,
                           const shader_cache_key& content_id) noexcept;

} // namespace granit::tools

#endif
