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
constexpr std::uint32_t schema = 2;
constexpr std::size_t header_size = 112;
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

} // namespace

shader_cache_key make_shader_cache_key(const shader_cache_context& context) noexcept {
  sha256_context hash;
  constexpr std::string_view domain = "granit-shader-cache-v1";
  update_cache_field(hash, domain);
  update_cache_field(hash, context.wgsl);
  update_cache_field(hash, context.entry_point);
  update_cache_field(hash, context.stage);
  update_cache_field(hash, context.tint_revision);
  update_cache_field(hash, context.target_environment);
  update_cache_field(hash, context.compile_options);
  return hash.finish();
}

shader_asset_error encode_shader_asset(const shader_asset_source& source,
                                       std::vector<std::byte>& output) noexcept {
  if (source.wgsl.empty() || source.spirv.empty() || source.spirv.size() % 4 != 0 ||
      source.reflection_json.empty())
    return shader_asset_error::invalid_argument;
  const auto maximum = std::numeric_limits<std::size_t>::max() - header_size;
  if (source.wgsl.size() > maximum || source.spirv.size() > maximum - source.wgsl.size() ||
      source.reflection_json.size() > maximum - source.wgsl.size() - source.spirv.size())
    return shader_asset_error::invalid_argument;
  try {
    const auto wgsl_offset = header_size;
    const auto spirv_offset = wgsl_offset + source.wgsl.size();
    const auto reflection_offset = spirv_offset + source.spirv.size();
    output.assign(reflection_offset + source.reflection_json.size(), std::byte{0});
    std::ranges::copy(magic, output.begin());
    write_u32(output, 8, schema);
    write_u32(output, 12, static_cast<std::uint32_t>(header_size));
    write_u64(output, 16, output.size());
    write_u64(output, 24, source.wgsl.size());
    write_u64(output, 32, source.spirv.size());
    write_u64(output, 40, source.reflection_json.size());
    std::ranges::copy(source.cache_key, output.begin() + cache_key_offset);
    std::memcpy(output.data() + wgsl_offset, source.wgsl.data(), source.wgsl.size());
    std::memcpy(output.data() + spirv_offset, source.spirv.data(), source.spirv.size());
    std::memcpy(output.data() + reflection_offset, source.reflection_json.data(),
                source.reflection_json.size());
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
  const auto wgsl_size = read_u64(bytes, 24);
  const auto spirv_size = read_u64(bytes, 32);
  const auto reflection_size = read_u64(bytes, 40);
  const auto wgsl_offset = static_cast<std::uint64_t>(header_size);
  const auto spirv_offset = wgsl_offset + wgsl_size;
  const auto reflection_offset = spirv_offset + spirv_size;
  if (!valid_section(wgsl_offset, wgsl_size, bytes.size()) || spirv_size % 4 != 0 ||
      !valid_section(spirv_offset, spirv_size, bytes.size()) ||
      !valid_section(reflection_offset, reflection_size, bytes.size()) ||
      reflection_offset + reflection_size != bytes.size())
    return shader_asset_error::invalid_layout;
  const auto expected = digest(bytes);
  if (!std::ranges::equal(expected, bytes.subspan(digest_offset, expected.size())))
    return shader_asset_error::digest_mismatch;
  output.wgsl = {reinterpret_cast<const char*>(bytes.data() + wgsl_offset),
                 static_cast<std::size_t>(wgsl_size)};
  output.spirv =
      bytes.subspan(static_cast<std::size_t>(spirv_offset), static_cast<std::size_t>(spirv_size));
  output.reflection_json = {reinterpret_cast<const char*>(bytes.data() + reflection_offset),
                            static_cast<std::size_t>(reflection_size)};
  std::ranges::copy(bytes.subspan(cache_key_offset, output.cache_key.size()),
                    output.cache_key.begin());
  return shader_asset_error::success;
}

shader_asset_error store_shader_asset(const std::filesystem::path& path,
                                      std::span<const std::byte> bytes, bool& cache_hit) noexcept {
  cache_hit = false;
  shader_asset_view view;
  if (decode_shader_asset(bytes, view) != shader_asset_error::success)
    return shader_asset_error::invalid_argument;
  try {
    const auto existing = read_file(path);
    if (std::ranges::equal(existing, bytes)) {
      cache_hit = true;
      return shader_asset_error::success;
    }
    std::error_code error;
    if (!path.parent_path().empty())
      std::filesystem::create_directories(path.parent_path(), error);
    if (error)
      return shader_asset_error::invalid_argument;
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
        return shader_asset_error::invalid_argument;
      stream.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
      stream.flush();
      if (!stream) {
        stream.close();
        std::filesystem::remove(temporary, error);
        return shader_asset_error::invalid_argument;
      }
    }
    if (!replace_file(temporary, path)) {
      std::filesystem::remove(temporary, error);
      if (std::ranges::equal(read_file(path), bytes)) {
        cache_hit = true;
        return shader_asset_error::success;
      }
      return shader_asset_error::invalid_argument;
    }
    return shader_asset_error::success;
  } catch (...) {
    return shader_asset_error::invalid_argument;
  }
}

} // namespace granit::tools
