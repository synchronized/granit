// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_FORMAT_DIGEST_H_
#define GRANIT_SHADER_FORMAT_DIGEST_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <granit/core/shader_types.hpp>

namespace granit::detail::shader_format {

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

shader_cache_key make_shader_cache_key(const shader_cache_context& context) noexcept;
shader_cache_key shader_bytes_sha256(std::span<const std::byte> bytes) noexcept;
shader_cache_key shader_bytes_sha256_zeroed(std::span<const std::byte> bytes, std::size_t offset,
                                            std::size_t size) noexcept;

} // namespace granit::detail::shader_format

#endif
