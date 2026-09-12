// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_FORMAT_SHADER_LIBRARY_H_
#define GRANIT_SHADER_FORMAT_SHADER_LIBRARY_H_

#include "shader_format/shader_object.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace granit::detail::shader_format {

enum class shader_library_error {
  success,
  invalid_argument,
  invalid_magic,
  unsupported_schema,
  invalid_layout,
  digest_mismatch,
  invalid_shader_object,
  missing_payload,
  conflicting_shader,
  conflicting_payload,
};

struct shader_library_object_source {
  std::span<const std::byte> manifest;
  std::span<const std::byte> wgsl;
  std::span<const std::byte> spirv;
};

struct shader_library_encode_desc {
  std::span<const shader_library_object_source> objects;
  granit_shader_backend_flags backend_mask = GRANIT_SHADER_BACKEND_ALL_BITS;
};

struct shader_library_variant {
  shader_object_backend backend{};
  shader_code_format code_format{};
  shader_profile profile{};
  std::uint64_t required_features = 0;
  std::uint32_t payload_index = 0;
  content_digest payload_digest{};

  friend bool operator==(const shader_library_variant&, const shader_library_variant&) = default;
};

struct shader_library_shader {
  shader_content_id content_id{};
  shader_cache_key cache_key{};
  shader_stage stage{};
  std::string_view entry_point;
  std::string_view reflection_json;
  std::vector<shader_library_variant> variants;
};

struct shader_library_payload {
  content_digest digest{};
  std::span<const std::byte> bytes;
};

struct shader_library_view {
  ::granit::content_digest content_digest{};
  granit_shader_backend_flags backend_mask = 0;
  std::vector<shader_library_shader> shaders;
  std::vector<shader_library_payload> payloads;
};

[[nodiscard]] shader_library_error encode_shader_library(const shader_library_encode_desc& desc,
                                                         std::vector<std::byte>& output) noexcept;
[[nodiscard]] shader_library_error decode_shader_library(std::span<const std::byte> bytes,
                                                         shader_library_view& output) noexcept;
[[nodiscard]] const shader_library_shader*
find_shader_library_shader(const shader_library_view& library,
                           const shader_content_id& content_id) noexcept;

} // namespace granit::detail::shader_format

#endif
