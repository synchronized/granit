// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/dynamic_uniform_arena.h"

#include <bit>
#include <limits>

namespace granit::pipeline::detail {
namespace {

bool add_overflows(std::uint64_t left, std::uint64_t right) noexcept {
  return right > std::numeric_limits<std::uint64_t>::max() - left;
}

} // namespace

uniform_arena_error dynamic_uniform_arena_plan::initialize(std::uint64_t alignment,
                                                           std::uint64_t max_binding_size,
                                                           std::uint64_t initial_capacity) noexcept {
  if (alignment == 0 || !std::has_single_bit(alignment))
    return uniform_arena_error::invalid_alignment;
  alignment_ = alignment;
  max_binding_size_ = max_binding_size;
  capacity_ = initial_capacity;
  cursor_ = 0;
  return uniform_arena_error::none;
}

uniform_arena_error
dynamic_uniform_arena_plan::allocate(std::uint64_t size,
                                     uniform_arena_allocation& output) noexcept {
  if (size == 0 || size > max_binding_size_)
    return uniform_arena_error::binding_too_large;
  const auto mask = alignment_ - 1;
  if (add_overflows(cursor_, mask))
    return uniform_arena_error::numeric_overflow;
  const auto offset = (cursor_ + mask) & ~mask;
  if (add_overflows(offset, size))
    return uniform_arena_error::numeric_overflow;
  const auto required = offset + size;
  if (required > capacity_) {
    auto grown = capacity_ == 0 ? alignment_ : capacity_;
    while (grown < required) {
      if (grown > std::numeric_limits<std::uint64_t>::max() / 2) {
        grown = required;
        break;
      }
      grown *= 2;
    }
    capacity_ = grown;
  }
  output = {.offset = offset, .size = size};
  cursor_ = required;
  return uniform_arena_error::none;
}

} // namespace granit::pipeline::detail
