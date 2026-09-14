// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_formats/shader/shader_object.h"

#include "core/sha256.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>

namespace granit::detail::shader_format {
namespace {

constexpr std::array magic{std::byte{'G'}, std::byte{'R'}, std::byte{'N'}, std::byte{'S'},
                           std::byte{'H'}, std::byte{'D'}, std::byte{'R'}, std::byte{0}};
constexpr std::uint32_t schema = 4;
constexpr std::size_t variant_offset = 112;
constexpr std::size_t variant_size = 64;
constexpr std::size_t maximum_variant_count = 2;
constexpr std::size_t header_size = variant_offset + variant_size * maximum_variant_count;
constexpr std::size_t digest_offset = 48;
constexpr std::size_t cache_key_offset = 80;

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  std::uint32_t value = 0;
  for (std::uint32_t index = 0; index < 4; ++index)
    value |= static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + index]))
             << (index * 8U);
  return value;
}

std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  std::uint64_t value = 0;
  for (std::uint32_t index = 0; index < 8; ++index)
    value |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(bytes[offset + index]))
             << (index * 8U);
  return value;
}

void write_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) noexcept {
  for (std::uint32_t index = 0; index < 4; ++index)
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
}

void write_u64(std::span<std::byte> bytes, std::size_t offset, std::uint64_t value) noexcept {
  for (std::uint32_t index = 0; index < 8; ++index)
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
}

std::array<std::byte, 32> digest(std::span<const std::byte> bytes) noexcept {
  return sha256_bytes_with_zeroed_range(bytes, digest_offset, 32);
}

bool valid_section(std::uint64_t offset, std::uint64_t size, std::uint64_t total) noexcept {
  return offset >= header_size && offset <= total && size <= total - offset;
}

void write_variant(std::span<std::byte> bytes, std::size_t index,
                   const shader_object_variant& variant) noexcept {
  const auto offset = variant_offset + variant_size * index;
  write_u32(bytes, offset, static_cast<std::uint32_t>(variant.backend));
  write_u32(bytes, offset + 4, static_cast<std::uint32_t>(variant.code_format));
  write_u32(bytes, offset + 8, static_cast<std::uint32_t>(variant.profile));
  write_u32(bytes, offset + 12, 0);
  write_u64(bytes, offset + 16, variant.required_features);
  write_u64(bytes, offset + 24, variant.byte_size);
  std::ranges::copy(variant.digest, bytes.begin() + static_cast<std::ptrdiff_t>(offset + 32));
}

bool read_variant(std::span<const std::byte> bytes, std::size_t index,
                  shader_object_variant& variant) noexcept {
  const auto offset = variant_offset + variant_size * index;
  variant.backend = static_cast<shader_object_backend>(read_u32(bytes, offset));
  variant.code_format = static_cast<shader_code_format>(read_u32(bytes, offset + 4));
  variant.profile = static_cast<shader_profile>(read_u32(bytes, offset + 8));
  variant.required_features = read_u64(bytes, offset + 16);
  variant.byte_size = read_u64(bytes, offset + 24);
  std::ranges::copy(bytes.subspan(offset + 32, variant.digest.size()), variant.digest.begin());
  const auto valid_backend = variant.backend == shader_object_backend::webgpu ||
                             variant.backend == shader_object_backend::vulkan;
  const auto valid_format = variant.code_format == shader_code_format::wgsl ||
                            variant.code_format == shader_code_format::spirv;
  return valid_backend && valid_format && variant.profile == shader_profile::portable &&
         variant.byte_size != 0 && read_u32(bytes, offset + 12) == 0;
}

} // namespace

shader_object_error encode_shader_object(const shader_object_source& source,
                                         std::vector<std::byte>& output) noexcept {
  if (source.wgsl.empty() || source.spirv.empty() || source.spirv.size() % 4 != 0 ||
      source.reflection_json.empty() || source.backend_mask == 0 ||
      (source.backend_mask & ~GRANIT_SHADER_BACKEND_ALL_BITS) != 0 ||
      (source.stage != shader_stage::vertex && source.stage != shader_stage::fragment &&
       source.stage != shader_stage::compute) ||
      source.entry_point.empty() || source.entry_point.size() > UINT32_MAX)
    return shader_object_error::invalid_argument;
  const auto maximum = std::numeric_limits<std::size_t>::max() - header_size;
  if (source.reflection_json.size() > maximum ||
      source.entry_point.size() > maximum - source.reflection_json.size())
    return shader_object_error::invalid_argument;
  try {
    const auto reflection_offset = header_size;
    const auto entry_point_offset = reflection_offset + source.reflection_json.size();
    output.assign(entry_point_offset + source.entry_point.size(), std::byte{0});
    std::ranges::copy(magic, output.begin());
    write_u32(output, 8, schema);
    write_u32(output, 12, static_cast<std::uint32_t>(header_size));
    write_u64(output, 16, output.size());
    write_u64(output, 24, source.reflection_json.size());
    const auto variant_count = static_cast<std::uint32_t>(std::popcount(source.backend_mask));
    write_u32(output, 32, variant_count);
    write_u32(output, 36, static_cast<std::uint32_t>(source.stage));
    write_u32(output, 40, static_cast<std::uint32_t>(source.entry_point.size()));
    std::ranges::copy(source.cache_key, output.begin() + cache_key_offset);
    const auto wgsl_bytes =
        std::span{reinterpret_cast<const std::byte*>(source.wgsl.data()), source.wgsl.size()};
    std::size_t variant_index = 0;
    if ((source.backend_mask & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0) {
      write_variant(output, variant_index++,
                    {.backend = shader_object_backend::webgpu,
                     .code_format = shader_code_format::wgsl,
                     .profile = shader_profile::portable,
                     .required_features = source.required_features,
                     .byte_size = source.wgsl.size(),
                     .digest = sha256_bytes(wgsl_bytes)});
    }
    if ((source.backend_mask & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0) {
      write_variant(output, variant_index,
                    {.backend = shader_object_backend::vulkan,
                     .code_format = shader_code_format::spirv,
                     .profile = shader_profile::portable,
                     .required_features = source.required_features,
                     .byte_size = source.spirv.size(),
                     .digest = sha256_bytes(source.spirv)});
    }
    std::memcpy(output.data() + reflection_offset, source.reflection_json.data(),
                source.reflection_json.size());
    std::memcpy(output.data() + entry_point_offset, source.entry_point.data(),
                source.entry_point.size());
    const auto hash = digest(output);
    std::ranges::copy(hash, output.begin() + digest_offset);
    return shader_object_error::success;
  } catch (...) {
    output.clear();
    return shader_object_error::invalid_argument;
  }
}

shader_object_error decode_shader_object(std::span<const std::byte> bytes,
                                         shader_object_view& output) noexcept {
  output = {};
  if (bytes.size() < header_size)
    return shader_object_error::invalid_layout;
  if (!std::ranges::equal(magic, bytes.first(magic.size())))
    return shader_object_error::invalid_magic;
  if (read_u32(bytes, 8) != schema)
    return shader_object_error::unsupported_schema;
  if (read_u32(bytes, 12) != header_size || read_u64(bytes, 16) != bytes.size())
    return shader_object_error::invalid_layout;
  const auto reflection_size = read_u64(bytes, 24);
  const auto variant_count = read_u32(bytes, 32);
  const auto stage = read_u32(bytes, 36);
  const auto entry_point_size = read_u32(bytes, 40);
  const auto reflection_offset = static_cast<std::uint64_t>(header_size);
  const auto entry_point_offset = reflection_offset + reflection_size;
  if (variant_count == 0 || variant_count > maximum_variant_count || stage < 1 || stage > 3 ||
      entry_point_size == 0 || !valid_section(reflection_offset, reflection_size, bytes.size()) ||
      !valid_section(entry_point_offset, entry_point_size, bytes.size()) ||
      entry_point_offset + entry_point_size != bytes.size())
    return shader_object_error::invalid_layout;
  const auto expected = digest(bytes);
  if (!std::ranges::equal(expected, bytes.subspan(digest_offset, expected.size())))
    return shader_object_error::digest_mismatch;
  output.reflection_json = {reinterpret_cast<const char*>(bytes.data() + reflection_offset),
                            static_cast<std::size_t>(reflection_size)};
  output.stage = static_cast<shader_stage>(stage);
  output.entry_point = {reinterpret_cast<const char*>(bytes.data() + entry_point_offset),
                        entry_point_size};
  std::ranges::copy(bytes.subspan(digest_offset, output.content_id.size()),
                    output.content_id.begin());
  std::ranges::copy(bytes.subspan(cache_key_offset, output.cache_key.size()),
                    output.cache_key.begin());
  output.variant_count = variant_count;
  for (std::uint32_t index = 0; index < variant_count; ++index) {
    if (!read_variant(bytes, index, output.variants[index]))
      return shader_object_error::invalid_layout;
    for (std::uint32_t previous = 0; previous < index; ++previous) {
      if (output.variants[previous].backend == output.variants[index].backend &&
          output.variants[previous].profile == output.variants[index].profile)
        return shader_object_error::invalid_layout;
    }
  }
  return shader_object_error::success;
}

const shader_object_variant* find_shader_object_variant(const shader_object_view& object,
                                                        shader_object_backend backend,
                                                        shader_profile profile) noexcept {
  for (std::uint32_t index = 0; index < object.variant_count; ++index) {
    if (object.variants[index].backend == backend && object.variants[index].profile == profile)
      return &object.variants[index];
  }
  return nullptr;
}

shader_object_error validate_shader_object_payloads(const shader_object_view& object,
                                                    std::string_view wgsl,
                                                    std::span<const std::byte> spirv) noexcept {
  const auto* wgsl_variant =
      find_shader_object_variant(object, shader_object_backend::webgpu, shader_profile::portable);
  const auto* spirv_variant =
      find_shader_object_variant(object, shader_object_backend::vulkan, shader_profile::portable);
  if ((wgsl_variant != nullptr && (wgsl_variant->code_format != shader_code_format::wgsl ||
                                   wgsl.size() != wgsl_variant->byte_size)) ||
      (spirv_variant != nullptr &&
       (spirv_variant->code_format != shader_code_format::spirv ||
        spirv.size() != spirv_variant->byte_size || spirv.size() % 4 != 0)))
    return shader_object_error::invalid_layout;
  const auto wgsl_bytes = std::span{reinterpret_cast<const std::byte*>(wgsl.data()), wgsl.size()};
  return (wgsl_variant == nullptr || sha256_bytes(wgsl_bytes) == wgsl_variant->digest) &&
                 (spirv_variant == nullptr || sha256_bytes(spirv) == spirv_variant->digest)
             ? shader_object_error::success
             : shader_object_error::digest_mismatch;
}

shader_object_error validate_shader_object_payload(const shader_object_view& object,
                                                   shader_object_backend backend,
                                                   std::span<const std::byte> payload) noexcept {
  const auto* variant = find_shader_object_variant(object, backend, shader_profile::portable);
  if (variant == nullptr)
    return shader_object_error::invalid_argument;
  const auto expected_format = backend == shader_object_backend::vulkan ? shader_code_format::spirv
                                                                        : shader_code_format::wgsl;
  if (variant->code_format != expected_format || payload.empty() ||
      payload.size() != variant->byte_size ||
      (expected_format == shader_code_format::spirv && payload.size() % 4 != 0))
    return shader_object_error::invalid_layout;
  return sha256_bytes(payload) == variant->digest ? shader_object_error::success
                                                  : shader_object_error::digest_mismatch;
}

} // namespace granit::detail::shader_format
