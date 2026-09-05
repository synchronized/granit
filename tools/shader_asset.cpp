// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_asset.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstring>
#include <fstream>
#include <limits>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

namespace granit::tools {
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

class sha256_context {
public:
  void update(std::span<const std::byte> bytes) noexcept {
    total_size_ += bytes.size();
    for (const auto value : bytes) {
      block_[block_size_++] = value;
      if (block_size_ == block_.size()) {
        transform();
        block_size_ = 0;
      }
    }
  }

  std::array<std::byte, 32> finish() noexcept {
    const auto bit_size = total_size_ * 8U;
    block_[block_size_++] = std::byte{0x80};
    if (block_size_ > 56) {
      std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_), block_.end(),
                std::byte{0});
      transform();
      block_size_ = 0;
    }
    std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_), block_.begin() + 56,
              std::byte{0});
    for (std::uint32_t index = 0; index < 8; ++index)
      block_[63 - index] = static_cast<std::byte>(bit_size >> (index * 8U));
    transform();
    std::array<std::byte, 32> result{};
    for (std::size_t index = 0; index < state_.size(); ++index) {
      result[index * 4] = static_cast<std::byte>(state_[index] >> 24U);
      result[index * 4 + 1] = static_cast<std::byte>(state_[index] >> 16U);
      result[index * 4 + 2] = static_cast<std::byte>(state_[index] >> 8U);
      result[index * 4 + 3] = static_cast<std::byte>(state_[index]);
    }
    return result;
  }

private:
  void transform() noexcept {
    static constexpr std::array constants{
        UINT32_C(0x428a2f98), UINT32_C(0x71374491), UINT32_C(0xb5c0fbcf), UINT32_C(0xe9b5dba5),
        UINT32_C(0x3956c25b), UINT32_C(0x59f111f1), UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5),
        UINT32_C(0xd807aa98), UINT32_C(0x12835b01), UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
        UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe), UINT32_C(0x9bdc06a7), UINT32_C(0xc19bf174),
        UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786), UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc),
        UINT32_C(0x2de92c6f), UINT32_C(0x4a7484aa), UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
        UINT32_C(0x983e5152), UINT32_C(0xa831c66d), UINT32_C(0xb00327c8), UINT32_C(0xbf597fc7),
        UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147), UINT32_C(0x06ca6351), UINT32_C(0x14292967),
        UINT32_C(0x27b70a85), UINT32_C(0x2e1b2138), UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
        UINT32_C(0x650a7354), UINT32_C(0x766a0abb), UINT32_C(0x81c2c92e), UINT32_C(0x92722c85),
        UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b), UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3),
        UINT32_C(0xd192e819), UINT32_C(0xd6990624), UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
        UINT32_C(0x19a4c116), UINT32_C(0x1e376c08), UINT32_C(0x2748774c), UINT32_C(0x34b0bcb5),
        UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a), UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3),
        UINT32_C(0x748f82ee), UINT32_C(0x78a5636f), UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
        UINT32_C(0x90befffa), UINT32_C(0xa4506ceb), UINT32_C(0xbef9a3f7), UINT32_C(0xc67178f2)};
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0; index < 16; ++index) {
      const auto offset = index * 4;
      words[index] = (std::to_integer<std::uint32_t>(block_[offset]) << 24U) |
                     (std::to_integer<std::uint32_t>(block_[offset + 1]) << 16U) |
                     (std::to_integer<std::uint32_t>(block_[offset + 2]) << 8U) |
                     std::to_integer<std::uint32_t>(block_[offset + 3]);
    }
    for (std::size_t index = 16; index < words.size(); ++index) {
      const auto s0 = std::rotr(words[index - 15], 7) ^ std::rotr(words[index - 15], 18) ^
                      (words[index - 15] >> 3U);
      const auto s1 = std::rotr(words[index - 2], 17) ^ std::rotr(words[index - 2], 19) ^
                      (words[index - 2] >> 10U);
      words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }
    auto [a, b, c, d, e, f, g, h] = state_;
    for (std::size_t index = 0; index < words.size(); ++index) {
      const auto sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
      const auto choice = (e & f) ^ (~e & g);
      const auto temporary1 = h + sum1 + choice + constants[index] + words[index];
      const auto sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
      const auto temporary2 = sum0 + ((a & b) ^ (a & c) ^ (b & c));
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  std::array<std::uint32_t, 8> state_{
      UINT32_C(0x6a09e667), UINT32_C(0xbb67ae85), UINT32_C(0x3c6ef372), UINT32_C(0xa54ff53a),
      UINT32_C(0x510e527f), UINT32_C(0x9b05688c), UINT32_C(0x1f83d9ab), UINT32_C(0x5be0cd19)};
  std::array<std::byte, 64> block_{};
  std::size_t block_size_ = 0;
  std::uint64_t total_size_ = 0;
};

std::array<std::byte, 32> digest(std::span<const std::byte> bytes) noexcept {
  sha256_context context;
  context.update(bytes.first(digest_offset));
  constexpr std::array<std::byte, 32> zeros{};
  context.update(zeros);
  context.update(bytes.subspan(digest_offset + zeros.size()));
  return context.finish();
}

std::array<std::byte, 32> payload_digest(std::span<const std::byte> bytes) noexcept {
  sha256_context context;
  context.update(bytes);
  return context.finish();
}

void update_cache_field(sha256_context& context, std::string_view value) noexcept {
  std::array<std::byte, 8> size{};
  const auto field_size = static_cast<std::uint64_t>(value.size());
  for (std::uint32_t index = 0; index < size.size(); ++index)
    size[index] = static_cast<std::byte>(field_size >> (index * 8U));
  context.update(size);
  context.update({reinterpret_cast<const std::byte*>(value.data()), value.size()});
}

bool valid_section(std::uint64_t offset, std::uint64_t size, std::uint64_t total) noexcept {
  return offset >= header_size && offset <= total && size <= total - offset;
}

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return {};
  const auto size = stream.tellg();
  if (size <= 0)
    return {};
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), size);
  return stream ? bytes : std::vector<std::byte>{};
}

bool replace_file(const std::filesystem::path& source, const std::filesystem::path& target) {
#if defined(_WIN32)
  return MoveFileExW(source.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code error;
  std::filesystem::rename(source, target, error);
  return !error;
#endif
}

bool write_file_atomically(const std::filesystem::path& path, std::span<const std::byte> bytes) {
  static std::atomic<std::uint64_t> sequence{0};
#if defined(_WIN32)
  const auto process_id = static_cast<std::uint64_t>(_getpid());
#else
  const auto process_id = static_cast<std::uint64_t>(getpid());
#endif
  auto temporary = path;
  temporary += ".tmp." + std::to_string(process_id) + "." +
               std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream)
      return false;
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    stream.flush();
    if (!stream) {
      stream.close();
      std::error_code error;
      std::filesystem::remove(temporary, error);
      return false;
    }
  }
  if (replace_file(temporary, path))
    return true;
  std::error_code error;
  std::filesystem::remove(temporary, error);
  return std::ranges::equal(read_file(path), bytes);
}

std::filesystem::path sidecar_path(const std::filesystem::path& manifest, std::string_view suffix) {
  auto result = manifest;
  result += suffix;
  return result;
}

void write_variant(std::span<std::byte> bytes, std::size_t index,
                   const shader_asset_variant& variant) noexcept {
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
                  shader_asset_variant& variant) noexcept {
  const auto offset = variant_offset + variant_size * index;
  variant.backend = static_cast<shader_asset_backend>(read_u32(bytes, offset));
  variant.code_format = static_cast<shader_asset_code_format>(read_u32(bytes, offset + 4));
  variant.profile = static_cast<shader_asset_profile>(read_u32(bytes, offset + 8));
  variant.required_features = read_u64(bytes, offset + 16);
  variant.byte_size = read_u64(bytes, offset + 24);
  std::ranges::copy(bytes.subspan(offset + 32, variant.digest.size()), variant.digest.begin());
  const auto valid_backend = variant.backend == shader_asset_backend::webgpu ||
                             variant.backend == shader_asset_backend::vulkan;
  const auto valid_format = variant.code_format == shader_asset_code_format::wgsl ||
                            variant.code_format == shader_asset_code_format::spirv;
  return valid_backend && valid_format && variant.profile == shader_asset_profile::portable &&
         variant.byte_size != 0 && read_u32(bytes, offset + 12) == 0;
}

} // namespace

shader_cache_key make_shader_cache_key(const shader_cache_context& context) noexcept {
  sha256_context hash;
  constexpr std::string_view domain = "granit-shader-cache-v3";
  update_cache_field(hash, domain);
  update_cache_field(hash, context.source_language);
  update_cache_field(hash, context.source);
  update_cache_field(hash, context.entry_point);
  update_cache_field(hash, context.stage);
  update_cache_field(hash, context.tint_revision);
  update_cache_field(hash, context.target_environment);
  update_cache_field(hash, context.compile_options);
  std::array<std::byte, 8> features{};
  for (std::uint32_t index = 0; index < features.size(); ++index)
    features[index] = static_cast<std::byte>(context.required_features >> (index * 8U));
  hash.update(features);
  return hash.finish();
}

std::string shader_file_sha256(const std::filesystem::path& path) noexcept {
  try {
    const auto bytes = read_file(path);
    if (bytes.empty())
      return {};
    const auto hash = payload_digest(bytes);
    constexpr char digits[] = "0123456789abcdef";
    std::string result(hash.size() * 2, '0');
    for (std::size_t index = 0; index < hash.size(); ++index) {
      const auto value = std::to_integer<unsigned char>(hash[index]);
      result[index * 2] = digits[value >> 4U];
      result[index * 2 + 1] = digits[value & 0x0fU];
    }
    return result;
  } catch (...) {
    return {};
  }
}

shader_asset_error encode_shader_asset(const shader_asset_source& source,
                                       std::vector<std::byte>& output) noexcept {
  if (source.wgsl.empty() || source.spirv.empty() || source.spirv.size() % 4 != 0 ||
      source.reflection_json.empty() || source.backend_mask == 0 ||
      (source.backend_mask & ~UINT32_C(3)) != 0 || source.stage < 1 || source.stage > 3 ||
      source.entry_point.empty() || source.entry_point.size() > UINT32_MAX)
    return shader_asset_error::invalid_argument;
  const auto maximum = std::numeric_limits<std::size_t>::max() - header_size;
  if (source.reflection_json.size() > maximum ||
      source.entry_point.size() > maximum - source.reflection_json.size())
    return shader_asset_error::invalid_argument;
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
    write_u32(output, 36, source.stage);
    write_u32(output, 40, static_cast<std::uint32_t>(source.entry_point.size()));
    std::ranges::copy(source.cache_key, output.begin() + cache_key_offset);
    const auto wgsl_bytes =
        std::span{reinterpret_cast<const std::byte*>(source.wgsl.data()), source.wgsl.size()};
    std::size_t variant_index = 0;
    if ((source.backend_mask & UINT32_C(2)) != 0) {
      write_variant(output, variant_index++,
                    {.backend = shader_asset_backend::webgpu,
                     .code_format = shader_asset_code_format::wgsl,
                     .profile = shader_asset_profile::portable,
                     .required_features = source.required_features,
                     .byte_size = source.wgsl.size(),
                     .digest = payload_digest(wgsl_bytes)});
    }
    if ((source.backend_mask & UINT32_C(1)) != 0) {
      write_variant(output, variant_index,
                    {.backend = shader_asset_backend::vulkan,
                     .code_format = shader_asset_code_format::spirv,
                     .profile = shader_asset_profile::portable,
                     .required_features = source.required_features,
                     .byte_size = source.spirv.size(),
                     .digest = payload_digest(source.spirv)});
    }
    std::memcpy(output.data() + reflection_offset, source.reflection_json.data(),
                source.reflection_json.size());
    std::memcpy(output.data() + entry_point_offset, source.entry_point.data(),
                source.entry_point.size());
    const auto hash = digest(output);
    std::ranges::copy(hash, output.begin() + digest_offset);
    return shader_asset_error::success;
  } catch (...) {
    output.clear();
    return shader_asset_error::invalid_argument;
  }
}

shader_asset_error decode_shader_asset(std::span<const std::byte> bytes,
                                       shader_asset_view& output) noexcept {
  output = {};
  if (bytes.size() < header_size)
    return shader_asset_error::invalid_layout;
  if (!std::ranges::equal(magic, bytes.first(magic.size())))
    return shader_asset_error::invalid_magic;
  if (read_u32(bytes, 8) != schema)
    return shader_asset_error::unsupported_schema;
  if (read_u32(bytes, 12) != header_size || read_u64(bytes, 16) != bytes.size())
    return shader_asset_error::invalid_layout;
  const auto reflection_size = read_u64(bytes, 24);
  const auto variant_count = read_u32(bytes, 32);
  const auto stage = read_u32(bytes, 36);
  const auto entry_point_size = read_u32(bytes, 40);
  const auto reflection_offset = static_cast<std::uint64_t>(header_size);
  const auto entry_point_offset = reflection_offset + reflection_size;
  if (variant_count == 0 || variant_count > maximum_variant_count || stage < 1 || stage > 3 ||
      entry_point_size == 0 ||
      !valid_section(reflection_offset, reflection_size, bytes.size()) ||
      !valid_section(entry_point_offset, entry_point_size, bytes.size()) ||
      entry_point_offset + entry_point_size != bytes.size())
    return shader_asset_error::invalid_layout;
  const auto expected = digest(bytes);
  if (!std::ranges::equal(expected, bytes.subspan(digest_offset, expected.size())))
    return shader_asset_error::digest_mismatch;
  output.reflection_json = {reinterpret_cast<const char*>(bytes.data() + reflection_offset),
                            static_cast<std::size_t>(reflection_size)};
  output.stage = stage;
  output.entry_point = {reinterpret_cast<const char*>(bytes.data() + entry_point_offset),
                        entry_point_size};
  std::ranges::copy(bytes.subspan(digest_offset, output.content_id.size()),
                    output.content_id.begin());
  std::ranges::copy(bytes.subspan(cache_key_offset, output.cache_key.size()),
                    output.cache_key.begin());
  output.variant_count = variant_count;
  for (std::uint32_t index = 0; index < variant_count; ++index) {
    if (!read_variant(bytes, index, output.variants[index]))
      return shader_asset_error::invalid_layout;
    for (std::uint32_t previous = 0; previous < index; ++previous) {
      if (output.variants[previous].backend == output.variants[index].backend &&
          output.variants[previous].profile == output.variants[index].profile)
        return shader_asset_error::invalid_layout;
    }
  }
  return shader_asset_error::success;
}

const shader_asset_variant* find_shader_asset_variant(const shader_asset_view& asset,
                                                      shader_asset_backend backend,
                                                      shader_asset_profile profile) noexcept {
  for (std::uint32_t index = 0; index < asset.variant_count; ++index) {
    if (asset.variants[index].backend == backend && asset.variants[index].profile == profile)
      return &asset.variants[index];
  }
  return nullptr;
}

shader_asset_error validate_shader_asset_payloads(const shader_asset_view& asset,
                                                  std::string_view wgsl,
                                                  std::span<const std::byte> spirv) noexcept {
  const auto* wgsl_variant = find_shader_asset_variant(asset, shader_asset_backend::webgpu,
                                                       shader_asset_profile::portable);
  const auto* spirv_variant = find_shader_asset_variant(asset, shader_asset_backend::vulkan,
                                                        shader_asset_profile::portable);
  if ((wgsl_variant != nullptr && (wgsl_variant->code_format != shader_asset_code_format::wgsl ||
                                   wgsl.size() != wgsl_variant->byte_size)) ||
      (spirv_variant != nullptr &&
       (spirv_variant->code_format != shader_asset_code_format::spirv ||
        spirv.size() != spirv_variant->byte_size || spirv.size() % 4 != 0)))
    return shader_asset_error::invalid_layout;
  const auto wgsl_bytes = std::span{reinterpret_cast<const std::byte*>(wgsl.data()), wgsl.size()};
  return (wgsl_variant == nullptr || payload_digest(wgsl_bytes) == wgsl_variant->digest) &&
                 (spirv_variant == nullptr || payload_digest(spirv) == spirv_variant->digest)
             ? shader_asset_error::success
             : shader_asset_error::digest_mismatch;
}

shader_asset_error store_shader_asset(const std::filesystem::path& path,
                                      std::span<const std::byte> manifest, std::string_view wgsl,
                                      std::span<const std::byte> spirv, bool& cache_hit) noexcept {
  cache_hit = false;
  shader_asset_view view;
  if (decode_shader_asset(manifest, view) != shader_asset_error::success ||
      validate_shader_asset_payloads(view, wgsl, spirv) != shader_asset_error::success)
    return shader_asset_error::invalid_argument;
  try {
    const auto wgsl_path = sidecar_path(path, ".wgsl");
    const auto spirv_path = sidecar_path(path, ".spv");
    const auto wgsl_bytes = std::span{reinterpret_cast<const std::byte*>(wgsl.data()), wgsl.size()};
    const auto has_wgsl = find_shader_asset_variant(view, shader_asset_backend::webgpu,
                                                    shader_asset_profile::portable) != nullptr;
    const auto has_spirv = find_shader_asset_variant(view, shader_asset_backend::vulkan,
                                                     shader_asset_profile::portable) != nullptr;
    if (std::ranges::equal(read_file(path), manifest) &&
        (!has_wgsl || std::ranges::equal(read_file(wgsl_path), wgsl_bytes)) &&
        (!has_spirv || std::ranges::equal(read_file(spirv_path), spirv)) &&
        (has_wgsl || !std::filesystem::exists(wgsl_path)) &&
        (has_spirv || !std::filesystem::exists(spirv_path))) {
      cache_hit = true;
      return shader_asset_error::success;
    }
    std::error_code error;
    if (!path.parent_path().empty())
      std::filesystem::create_directories(path.parent_path(), error);
    if (error)
      return shader_asset_error::invalid_argument;
    // 清单最后替换；中途失败时旧清单不会错误匹配新旧混合载荷。
    if ((has_wgsl && !write_file_atomically(wgsl_path, wgsl_bytes)) ||
        (has_spirv && !write_file_atomically(spirv_path, spirv)))
      return shader_asset_error::invalid_argument;
    if (!has_wgsl)
      std::filesystem::remove(wgsl_path, error);
    if (!has_spirv)
      std::filesystem::remove(spirv_path, error);
    if (error || !write_file_atomically(path, manifest))
      return shader_asset_error::invalid_argument;
    return shader_asset_error::success;
  } catch (...) {
    return shader_asset_error::invalid_argument;
  }
}

} // namespace granit::tools
