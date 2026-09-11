// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_format/digest.h"

#include <algorithm>
#include <array>
#include <bit>

namespace granit::detail::shader_format {
namespace {

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

shader_cache_key shader_bytes_sha256(std::span<const std::byte> bytes) noexcept {
  return payload_digest(bytes);
}

shader_cache_key shader_bytes_sha256_zeroed(std::span<const std::byte> bytes, std::size_t offset,
                                            std::size_t size) noexcept {
  if (offset > bytes.size() || size > bytes.size() - offset)
    return {};
  const auto suffix_offset = offset + size;
  sha256_context context;
  context.update(bytes.first(offset));
  constexpr std::array<std::byte, 64> zeros{};
  while (size != 0) {
    const auto chunk = std::min(size, zeros.size());
    context.update(std::span{zeros}.first(chunk));
    size -= chunk;
  }
  context.update(bytes.subspan(suffix_offset));
  return context.finish();
}

} // namespace granit::detail::shader_format
