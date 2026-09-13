// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/sha256.h"

#include <array>
#include <span>
#include <string_view>

#include <catch2/catch_all.hpp>

namespace {

std::span<const std::byte> bytes(std::string_view value) {
  return {reinterpret_cast<const std::byte*>(value.data()), value.size()};
}

} // namespace

TEST_CASE("SHA-256 使用标准摘要") {
  constexpr std::array expected{
      std::byte{0xba}, std::byte{0x78}, std::byte{0x16}, std::byte{0xbf}, std::byte{0x8f},
      std::byte{0x01}, std::byte{0xcf}, std::byte{0xea}, std::byte{0x41}, std::byte{0x41},
      std::byte{0x40}, std::byte{0xde}, std::byte{0x5d}, std::byte{0xae}, std::byte{0x22},
      std::byte{0x23}, std::byte{0xb0}, std::byte{0x03}, std::byte{0x61}, std::byte{0xa3},
      std::byte{0x96}, std::byte{0x17}, std::byte{0x7a}, std::byte{0x9c}, std::byte{0xb4},
      std::byte{0x10}, std::byte{0xff}, std::byte{0x61}, std::byte{0xf2}, std::byte{0x00},
      std::byte{0x15}, std::byte{0xad}};
  CHECK(granit::detail::sha256_bytes(bytes("abc")) == expected);
}

TEST_CASE("SHA-256 分段与清零范围保持确定性") {
  const std::array segments{bytes("ab"), bytes("c")};
  CHECK(granit::detail::sha256_bytes(segments) == granit::detail::sha256_bytes(bytes("abc")));
  CHECK(granit::detail::sha256_bytes_with_zeroed_range(bytes("abcdef"), 2, 2) ==
        granit::detail::sha256_bytes(bytes(std::string_view{"ab\0\0ef", 6})));
  CHECK(granit::detail::sha256_bytes_with_zeroed_range(bytes("abc"), 4, 0) ==
        granit::content_digest{});
}
