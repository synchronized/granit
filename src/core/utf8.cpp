// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "utf8.h"

#include <cstdint>

namespace granit::input::detail {
namespace {

bool continuation(std::uint8_t value) noexcept { return value >= 0x80 && value <= 0xbf; }

std::size_t code_point_length(std::string_view text, std::size_t offset) noexcept {
  const auto remaining = text.size() - offset;
  const auto first = static_cast<std::uint8_t>(text[offset]);
  if (first <= 0x7f)
    return 1;
  if (first >= 0xc2 && first <= 0xdf) {
    return remaining >= 2 && continuation(static_cast<std::uint8_t>(text[offset + 1])) ? 2 : 0;
  }
  if (first >= 0xe0 && first <= 0xef) {
    if (remaining < 3)
      return 0;
    const auto second = static_cast<std::uint8_t>(text[offset + 1]);
    const auto third = static_cast<std::uint8_t>(text[offset + 2]);
    const bool valid_second = first == 0xe0   ? second >= 0xa0 && second <= 0xbf
                              : first == 0xed ? second >= 0x80 && second <= 0x9f
                                              : continuation(second);
    return valid_second && continuation(third) ? 3 : 0;
  }
  if (first >= 0xf0 && first <= 0xf4) {
    if (remaining < 4)
      return 0;
    const auto second = static_cast<std::uint8_t>(text[offset + 1]);
    const bool valid_second = first == 0xf0   ? second >= 0x90 && second <= 0xbf
                              : first == 0xf4 ? second >= 0x80 && second <= 0x8f
                                              : continuation(second);
    return valid_second && continuation(static_cast<std::uint8_t>(text[offset + 2])) &&
                   continuation(static_cast<std::uint8_t>(text[offset + 3]))
               ? 4
               : 0;
  }
  return 0;
}

} // namespace

utf8_chunk_result next_utf8_chunk(std::string_view text, std::size_t capacity,
                                  std::size_t& length) noexcept {
  length = 0;
  while (length < text.size()) {
    const auto code_point = code_point_length(text, length);
    if (code_point == 0)
      return utf8_chunk_result::invalid;
    if (code_point > capacity - length)
      return length == 0 ? utf8_chunk_result::capacity_too_small : utf8_chunk_result::success;
    length += code_point;
  }
  return utf8_chunk_result::success;
}

} // namespace granit::input::detail
