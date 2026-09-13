// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_format/shader_cache_key.h"

#include "core/sha256.h"

#include <array>

namespace granit::detail::shader_format {
namespace {

std::span<const std::byte> as_bytes(std::string_view value) noexcept {
  return {reinterpret_cast<const std::byte*>(value.data()), value.size()};
}

void write_u64(std::array<std::byte, 8>& output, std::uint64_t value) noexcept {
  for (std::uint32_t index = 0; index < output.size(); ++index)
    output[index] = static_cast<std::byte>(value >> (index * 8U));
}

} // namespace

shader_cache_key make_shader_cache_key(const shader_cache_context& context) noexcept {
  constexpr std::string_view domain = "granit-shader-cache-v3";
  const std::array fields{domain, context.source_language, context.source, context.entry_point,
                          context.stage, context.tint_revision, context.target_environment,
                          context.compile_options};
  std::array<std::array<std::byte, 8>, fields.size() + 1> sizes{};
  std::array<std::span<const std::byte>, fields.size() * 2 + 1> segments{};
  for (std::size_t index = 0; index < fields.size(); ++index) {
    write_u64(sizes[index], fields[index].size());
    segments[index * 2] = sizes[index];
    segments[index * 2 + 1] = as_bytes(fields[index]);
  }
  write_u64(sizes.back(), context.required_features);
  segments.back() = sizes.back();
  return granit::detail::sha256_bytes(segments);
}

} // namespace granit::detail::shader_format
