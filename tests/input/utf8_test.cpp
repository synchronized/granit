// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "input/utf8.h"

#include <catch2/catch_all.hpp>

#include <string>
#include <string_view>

using granit::input::detail::next_utf8_chunk;
using granit::input::detail::utf8_chunk_result;

TEST_CASE("UTF-8 分片保持码点边界", "[input][utf8]") {
  std::size_t length = 99;
  CHECK(next_utf8_chunk({}, 48, length) == utf8_chunk_result::success);
  CHECK(length == 0);

  const std::string exact = std::string(44, 'a') + "😀";
  REQUIRE(exact.size() == 48);
  CHECK(next_utf8_chunk(exact, 48, length) == utf8_chunk_result::success);
  CHECK(length == 48);

  const std::string split = std::string(47, 'a') + "😀";
  CHECK(next_utf8_chunk(split, 48, length) == utf8_chunk_result::success);
  CHECK(length == 47);
  CHECK(next_utf8_chunk(std::string_view{split}.substr(length), 48, length) ==
        utf8_chunk_result::success);
  CHECK(length == 4);

  CHECK(next_utf8_chunk("😀", 3, length) == utf8_chunk_result::capacity_too_small);
  CHECK(length == 0);
}

TEST_CASE("UTF-8 分片拒绝非法序列", "[input][utf8]") {
  std::size_t length = 0;
  const std::string overlong{"\xc0\x80", 2};
  const std::string surrogate{"\xed\xa0\x80", 3};
  const std::string out_of_range{"\xf4\x90\x80\x80", 4};
  const std::string truncated{"\xe4\xb8", 2};

  CHECK(next_utf8_chunk(overlong, 48, length) == utf8_chunk_result::invalid);
  CHECK(next_utf8_chunk(surrogate, 48, length) == utf8_chunk_result::invalid);
  CHECK(next_utf8_chunk(out_of_range, 48, length) == utf8_chunk_result::invalid);
  CHECK(next_utf8_chunk(truncated, 48, length) == utf8_chunk_result::invalid);
}
