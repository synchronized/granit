// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INPUT_UTF8_H_
#define GRANIT_INPUT_UTF8_H_

#include <cstddef>
#include <string_view>

namespace granit::input::detail {

enum class utf8_chunk_result {
  success,
  invalid,
  capacity_too_small,
};

/** 返回不超过 capacity 且落在 UTF-8 码点边界上的首个分片长度。 */
[[nodiscard]] utf8_chunk_result next_utf8_chunk(std::string_view text, std::size_t capacity,
                                                std::size_t& length) noexcept;

} // namespace granit::input::detail

#endif
