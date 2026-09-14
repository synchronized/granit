// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_formats/shader/shader_library.h"

#include "core/sha256.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace granit::detail::shader_format {
namespace {

constexpr std::array magic{std::byte{'G'}, std::byte{'R'}, std::byte{'N'}, std::byte{'S'},
                           std::byte{'H'}, std::byte{'L'}, std::byte{'B'}, std::byte{0}};
constexpr std::uint32_t schema = 1;
// 固定表位于文件头之后，字符串区和八字节对齐的载荷区依次排列。
constexpr std::size_t header_size = 128;
constexpr std::size_t shader_record_size = 112;
constexpr std::size_t variant_record_size = 64;
constexpr std::size_t payload_record_size = 64;
constexpr std::size_t digest_offset = 80;

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

bool checked_add(std::size_t left, std::size_t right, std::size_t& result) noexcept {
  if (right > std::numeric_limits<std::size_t>::max() - left)
    return false;
  result = left + right;
  return true;
}

bool checked_table(std::size_t offset, std::size_t count, std::size_t record_size,
                   std::size_t& result) noexcept {
  if (count > (std::numeric_limits<std::size_t>::max() - offset) / record_size)
    return false;
  result = offset + count * record_size;
  return true;
}

bool align_eight(std::size_t value, std::size_t& result) noexcept {
  if (value > std::numeric_limits<std::size_t>::max() - 7)
    return false;
  result = (value + 7) & ~std::size_t{7};
  return true;
}

bool valid_range(std::uint64_t offset, std::uint64_t size, std::uint64_t lower,
                 std::uint64_t upper) noexcept {
  return offset >= lower && offset <= upper && size <= upper - offset;
}

bool zero_range(std::span<const std::byte> bytes) noexcept {
  return std::ranges::all_of(bytes, [](std::byte value) { return value == std::byte{0}; });
}

std::uint32_t backend_bit(shader_object_backend backend) noexcept {
  return backend == shader_object_backend::vulkan   ? GRANIT_SHADER_BACKEND_VULKAN_BIT
         : backend == shader_object_backend::webgpu ? GRANIT_SHADER_BACKEND_WEBGPU_BIT
                                                    : 0;
}

bool valid_variant(const shader_library_variant& variant) noexcept {
  const auto matching_format = (variant.backend == shader_object_backend::vulkan &&
                                variant.code_format == shader_code_format::spirv) ||
                               (variant.backend == shader_object_backend::webgpu &&
                                variant.code_format == shader_code_format::wgsl);
  return matching_format && variant.profile == shader_profile::portable;
}

struct encoded_shader {
  shader_content_id content_id{};
  shader_cache_key cache_key{};
  shader_stage stage{};
  std::string entry_point;
  std::string reflection_json;
  std::vector<shader_library_variant> variants;
};

struct encoded_payload {
  content_digest digest{};
  std::vector<std::byte> bytes;
};

bool same_shader(const encoded_shader& left, const encoded_shader& right) noexcept {
  return left.content_id == right.content_id && left.cache_key == right.cache_key &&
         left.stage == right.stage && left.entry_point == right.entry_point &&
         left.reflection_json == right.reflection_json && left.variants == right.variants;
}

shader_library_error add_payload(std::span<const std::byte> bytes, const content_digest& digest,
                                 std::vector<encoded_payload>& payloads) {
  const auto found = std::ranges::find(payloads, digest, &encoded_payload::digest);
  if (found != payloads.end())
    return std::ranges::equal(found->bytes, bytes) ? shader_library_error::success
                                                   : shader_library_error::conflicting_payload;
  payloads.push_back({digest, {bytes.begin(), bytes.end()}});
  return shader_library_error::success;
}

std::span<const std::byte> source_payload(const shader_library_object_source& source,
                                          shader_object_backend backend) noexcept {
  return backend == shader_object_backend::vulkan ? source.spirv : source.wgsl;
}

} // namespace

shader_library_error encode_shader_library(const shader_library_encode_desc& desc,
                                           std::vector<std::byte>& output) noexcept {
  output.clear();
  if (desc.objects.empty() || desc.backend_mask == 0 ||
      (desc.backend_mask & ~GRANIT_SHADER_BACKEND_ALL_BITS) != 0) {
    return shader_library_error::invalid_argument;
  }
  try {
    std::vector<encoded_shader> shaders;
    std::vector<encoded_payload> payloads;
    shaders.reserve(desc.objects.size());
    for (const auto& source : desc.objects) {
      shader_object_view object;
      if (decode_shader_object(source.manifest, object) != shader_object_error::success)
        return shader_library_error::invalid_shader_object;
      encoded_shader shader{.content_id = object.content_id,
                            .cache_key = object.cache_key,
                            .stage = object.stage,
                            .entry_point = std::string{object.entry_point},
                            .reflection_json = std::string{object.reflection_json},
                            .variants = {}};
      for (const auto backend : {shader_object_backend::webgpu, shader_object_backend::vulkan}) {
        if ((desc.backend_mask & backend_bit(backend)) == 0)
          continue;
        const auto* variant = find_shader_object_variant(object, backend, shader_profile::portable);
        const auto bytes = source_payload(source, backend);
        if (variant == nullptr || bytes.empty())
          return shader_library_error::missing_payload;
        if (validate_shader_object_payload(object, backend, bytes) != shader_object_error::success)
          return shader_library_error::invalid_shader_object;
        shader_library_variant library_variant{.backend = variant->backend,
                                               .code_format = variant->code_format,
                                               .profile = variant->profile,
                                               .required_features = variant->required_features,
                                               .payload_digest = variant->digest};
        if (!valid_variant(library_variant))
          return shader_library_error::invalid_shader_object;
        const auto payload_result = add_payload(bytes, variant->digest, payloads);
        if (payload_result != shader_library_error::success)
          return payload_result;
        shader.variants.push_back(library_variant);
      }
      std::ranges::sort(shader.variants, {}, &shader_library_variant::backend);
      shaders.push_back(std::move(shader));
    }
    // 内容身份决定记录顺序，使输入枚举顺序不影响最终 Library。
    std::ranges::sort(shaders, {}, &encoded_shader::content_id);
    for (std::size_t index = 1; index < shaders.size(); ++index) {
      if (shaders[index - 1].content_id == shaders[index].content_id &&
          !same_shader(shaders[index - 1], shaders[index])) {
        return shader_library_error::conflicting_shader;
      }
    }
    shaders.erase(std::unique(shaders.begin(), shaders.end(), same_shader), shaders.end());
    std::ranges::sort(payloads, {}, &encoded_payload::digest);
    if (shaders.size() > UINT32_MAX || payloads.size() > UINT32_MAX)
      return shader_library_error::invalid_argument;
    std::size_t variant_count = 0;
    std::size_t string_size = 0;
    for (const auto& shader : shaders) {
      if (!checked_add(variant_count, shader.variants.size(), variant_count) ||
          !checked_add(string_size, shader.reflection_json.size(), string_size) ||
          !checked_add(string_size, shader.entry_point.size(), string_size)) {
        return shader_library_error::invalid_argument;
      }
    }
    if (variant_count > UINT32_MAX)
      return shader_library_error::invalid_argument;
    std::size_t variant_offset = 0;
    std::size_t payload_offset = 0;
    std::size_t string_offset = 0;
    std::size_t payload_data_offset = 0;
    if (!checked_table(header_size, shaders.size(), shader_record_size, variant_offset) ||
        !checked_table(variant_offset, variant_count, variant_record_size, payload_offset) ||
        !checked_table(payload_offset, payloads.size(), payload_record_size, string_offset) ||
        !checked_add(string_offset, string_size, payload_data_offset) ||
        !align_eight(payload_data_offset, payload_data_offset)) {
      return shader_library_error::invalid_argument;
    }
    auto file_size = payload_data_offset;
    for (const auto& payload : payloads) {
      if (!align_eight(file_size, file_size) ||
          !checked_add(file_size, payload.bytes.size(), file_size)) {
        return shader_library_error::invalid_argument;
      }
    }
    output.assign(file_size, std::byte{0});
    std::ranges::copy(magic, output.begin());
    write_u32(output, 8, schema);
    write_u32(output, 12, static_cast<std::uint32_t>(header_size));
    write_u64(output, 16, file_size);
    write_u32(output, 24, static_cast<std::uint32_t>(shaders.size()));
    write_u32(output, 28, static_cast<std::uint32_t>(variant_count));
    write_u32(output, 32, static_cast<std::uint32_t>(payloads.size()));
    write_u32(output, 36, desc.backend_mask);
    write_u64(output, 40, header_size);
    write_u64(output, 48, variant_offset);
    write_u64(output, 56, payload_offset);
    write_u64(output, 64, string_offset);
    write_u64(output, 72, payload_data_offset);

    std::size_t next_variant = 0;
    std::size_t next_string = string_offset;
    for (std::size_t shader_index = 0; shader_index < shaders.size(); ++shader_index) {
      const auto& shader = shaders[shader_index];
      const auto record = header_size + shader_index * shader_record_size;
      std::ranges::copy(shader.content_id, output.begin() + static_cast<std::ptrdiff_t>(record));
      std::ranges::copy(shader.cache_key,
                        output.begin() + static_cast<std::ptrdiff_t>(record + 32));
      write_u32(output, record + 64, static_cast<std::uint32_t>(shader.stage));
      write_u64(output, record + 72, next_string);
      write_u64(output, record + 80, shader.reflection_json.size());
      std::memcpy(output.data() + next_string, shader.reflection_json.data(),
                  shader.reflection_json.size());
      next_string += shader.reflection_json.size();
      write_u64(output, record + 88, next_string);
      write_u64(output, record + 96, shader.entry_point.size());
      std::memcpy(output.data() + next_string, shader.entry_point.data(),
                  shader.entry_point.size());
      next_string += shader.entry_point.size();
      write_u32(output, record + 104, static_cast<std::uint32_t>(next_variant));
      write_u32(output, record + 108, static_cast<std::uint32_t>(shader.variants.size()));
      for (const auto& variant : shader.variants) {
        const auto variant_record = variant_offset + next_variant * variant_record_size;
        write_u32(output, variant_record, static_cast<std::uint32_t>(variant.backend));
        write_u32(output, variant_record + 4, static_cast<std::uint32_t>(variant.code_format));
        write_u32(output, variant_record + 8, static_cast<std::uint32_t>(variant.profile));
        write_u64(output, variant_record + 16, variant.required_features);
        const auto payload = std::ranges::lower_bound(payloads, variant.payload_digest, {},
                                                      &encoded_payload::digest);
        if (payload == payloads.end() || payload->digest != variant.payload_digest)
          return shader_library_error::invalid_argument;
        write_u32(output, variant_record + 24,
                  static_cast<std::uint32_t>(payload - payloads.begin()));
        std::ranges::copy(variant.payload_digest,
                          output.begin() + static_cast<std::ptrdiff_t>(variant_record + 32));
        ++next_variant;
      }
    }

    std::size_t next_payload = payload_data_offset;
    for (std::size_t index = 0; index < payloads.size(); ++index) {
      const auto& payload = payloads[index];
      if (!align_eight(next_payload, next_payload))
        return shader_library_error::invalid_argument;
      const auto record = payload_offset + index * payload_record_size;
      std::ranges::copy(payload.digest, output.begin() + static_cast<std::ptrdiff_t>(record));
      write_u64(output, record + 32, next_payload);
      write_u64(output, record + 40, payload.bytes.size());
      std::ranges::copy(payload.bytes, output.begin() + static_cast<std::ptrdiff_t>(next_payload));
      next_payload += payload.bytes.size();
    }
    const auto digest = sha256_bytes_with_zeroed_range(output, digest_offset, 32);
    std::ranges::copy(digest, output.begin() + static_cast<std::ptrdiff_t>(digest_offset));
    return shader_library_error::success;
  } catch (...) {
    output.clear();
    return shader_library_error::invalid_argument;
  }
}

shader_library_error decode_shader_library(std::span<const std::byte> bytes,
                                           shader_library_view& output) noexcept {
  output = {};
  if (bytes.size() < header_size)
    return shader_library_error::invalid_layout;
  if (!std::ranges::equal(magic, bytes.first(magic.size())))
    return shader_library_error::invalid_magic;
  if (read_u32(bytes, 8) != schema)
    return shader_library_error::unsupported_schema;
  const auto shader_count = read_u32(bytes, 24);
  const auto variant_count = read_u32(bytes, 28);
  const auto payload_count = read_u32(bytes, 32);
  const auto backend_mask = read_u32(bytes, 36);
  const auto shader_offset = read_u64(bytes, 40);
  const auto variant_offset = read_u64(bytes, 48);
  const auto payload_offset = read_u64(bytes, 56);
  const auto string_offset = read_u64(bytes, 64);
  const auto payload_data_offset = read_u64(bytes, 72);
  if (read_u32(bytes, 12) != header_size || read_u64(bytes, 16) != bytes.size() ||
      shader_count == 0 || variant_count == 0 || payload_count == 0 || backend_mask == 0 ||
      (backend_mask & ~GRANIT_SHADER_BACKEND_ALL_BITS) != 0 || shader_offset != header_size ||
      !zero_range(bytes.subspan(112, 16))) {
    return shader_library_error::invalid_layout;
  }
  std::size_t expected_variant = 0;
  std::size_t expected_payload = 0;
  std::size_t expected_string = 0;
  if (!checked_table(header_size, shader_count, shader_record_size, expected_variant) ||
      !checked_table(expected_variant, variant_count, variant_record_size, expected_payload) ||
      !checked_table(expected_payload, payload_count, payload_record_size, expected_string) ||
      variant_offset != expected_variant || payload_offset != expected_payload ||
      string_offset != expected_string || payload_data_offset < string_offset ||
      payload_data_offset > bytes.size() || payload_data_offset % 8 != 0) {
    return shader_library_error::invalid_layout;
  }
  const auto expected_digest = sha256_bytes_with_zeroed_range(bytes, digest_offset, 32);
  if (!std::ranges::equal(expected_digest, bytes.subspan(digest_offset, 32)))
    return shader_library_error::digest_mismatch;
  try {
    shader_library_view parsed;
    parsed.content_digest = expected_digest;
    parsed.backend_mask = backend_mask;
    parsed.shaders.reserve(shader_count);
    parsed.payloads.reserve(payload_count);
    std::size_t next_variant = 0;
    std::uint64_t next_string = string_offset;
    shader_content_id previous_shader{};
    for (std::size_t shader_index = 0; shader_index < shader_count; ++shader_index) {
      const auto record =
          static_cast<std::size_t>(shader_offset) + shader_index * shader_record_size;
      shader_library_shader shader;
      std::ranges::copy(bytes.subspan(record, 32), shader.content_id.begin());
      std::ranges::copy(bytes.subspan(record + 32, 32), shader.cache_key.begin());
      const auto stage = read_u32(bytes, record + 64);
      shader.stage = static_cast<shader_stage>(stage);
      const auto reflection_offset = read_u64(bytes, record + 72);
      const auto reflection_size = read_u64(bytes, record + 80);
      const auto entry_offset = read_u64(bytes, record + 88);
      const auto entry_size = read_u64(bytes, record + 96);
      const auto first_variant = read_u32(bytes, record + 104);
      const auto shader_variant_count = read_u32(bytes, record + 108);
      if ((shader.stage != shader_stage::vertex && shader.stage != shader_stage::fragment &&
           shader.stage != shader_stage::compute) ||
          reflection_size == 0 || entry_size == 0 || reflection_offset != next_string ||
          !valid_range(reflection_offset, reflection_size, string_offset, payload_data_offset) ||
          entry_offset != reflection_offset + reflection_size ||
          !valid_range(entry_offset, entry_size, string_offset, payload_data_offset) ||
          first_variant != next_variant || shader_variant_count == 0 ||
          next_variant > variant_count || shader_variant_count > variant_count - next_variant ||
          !zero_range(bytes.subspan(record + 68, 4)) ||
          (shader_index != 0 && !(previous_shader < shader.content_id))) {
        return shader_library_error::invalid_layout;
      }
      shader.reflection_json = {reinterpret_cast<const char*>(bytes.data() + reflection_offset),
                                static_cast<std::size_t>(reflection_size)};
      shader.entry_point = {reinterpret_cast<const char*>(bytes.data() + entry_offset),
                            static_cast<std::size_t>(entry_size)};
      next_string = entry_offset + entry_size;
      shader.variants.reserve(shader_variant_count);
      std::uint32_t previous_backend = 0;
      for (std::uint32_t index = 0; index < shader_variant_count; ++index) {
        const auto variant_record =
            static_cast<std::size_t>(variant_offset) + (next_variant + index) * variant_record_size;
        shader_library_variant variant{
            .backend = static_cast<shader_object_backend>(read_u32(bytes, variant_record)),
            .code_format = static_cast<shader_code_format>(read_u32(bytes, variant_record + 4)),
            .profile = static_cast<shader_profile>(read_u32(bytes, variant_record + 8)),
            .required_features = read_u64(bytes, variant_record + 16),
            .payload_index = read_u32(bytes, variant_record + 24)};
        std::ranges::copy(bytes.subspan(variant_record + 32, 32), variant.payload_digest.begin());
        const auto backend = static_cast<std::uint32_t>(variant.backend);
        if (!valid_variant(variant) || (backend_mask & backend_bit(variant.backend)) == 0 ||
            variant.payload_index >= payload_count || backend <= previous_backend ||
            read_u32(bytes, variant_record + 12) != 0 ||
            read_u32(bytes, variant_record + 28) != 0) {
          return shader_library_error::invalid_layout;
        }
        previous_backend = backend;
        shader.variants.push_back(variant);
      }
      next_variant += shader_variant_count;
      previous_shader = shader.content_id;
      parsed.shaders.push_back(std::move(shader));
    }
    if (next_variant != variant_count || next_string > payload_data_offset ||
        !zero_range(bytes.subspan(static_cast<std::size_t>(next_string),
                                  static_cast<std::size_t>(payload_data_offset - next_string)))) {
      return shader_library_error::invalid_layout;
    }
    std::uint64_t next_payload = payload_data_offset;
    content_digest previous_payload{};
    // 解码器要求规范顺序和连续范围，避免同一语义存在多种可接受编码。
    for (std::size_t index = 0; index < payload_count; ++index) {
      const auto record = static_cast<std::size_t>(payload_offset) + index * payload_record_size;
      shader_library_payload payload;
      std::ranges::copy(bytes.subspan(record, 32), payload.digest.begin());
      const auto offset = read_u64(bytes, record + 32);
      const auto size = read_u64(bytes, record + 40);
      std::size_t aligned = 0;
      if (!align_eight(static_cast<std::size_t>(next_payload), aligned) || offset != aligned ||
          size == 0 || !valid_range(offset, size, payload_data_offset, bytes.size()) ||
          !zero_range(bytes.subspan(static_cast<std::size_t>(next_payload),
                                    aligned - static_cast<std::size_t>(next_payload))) ||
          !zero_range(bytes.subspan(record + 48, 16)) ||
          (index != 0 && !(previous_payload < payload.digest))) {
        return shader_library_error::invalid_layout;
      }
      payload.bytes =
          bytes.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
      if (sha256_bytes(payload.bytes) != payload.digest)
        return shader_library_error::digest_mismatch;
      next_payload = offset + size;
      previous_payload = payload.digest;
      parsed.payloads.push_back(payload);
    }
    if (next_payload != bytes.size())
      return shader_library_error::invalid_layout;
    std::vector<bool> referenced(payload_count, false);
    for (const auto& shader : parsed.shaders) {
      for (const auto& variant : shader.variants) {
        const auto& payload = parsed.payloads[variant.payload_index];
        if (payload.digest != variant.payload_digest)
          return shader_library_error::invalid_layout;
        referenced[variant.payload_index] = true;
      }
    }
    if (std::ranges::find(referenced, false) != referenced.end())
      return shader_library_error::invalid_layout;
    output = std::move(parsed);
    return shader_library_error::success;
  } catch (...) {
    output = {};
    return shader_library_error::invalid_argument;
  }
}

const shader_library_shader*
find_shader_library_shader(const shader_library_view& library,
                           const shader_content_id& content_id) noexcept {
  const auto found =
      std::ranges::lower_bound(library.shaders, content_id, {}, &shader_library_shader::content_id);
  return found != library.shaders.end() && found->content_id == content_id ? &*found : nullptr;
}

} // namespace granit::detail::shader_format
