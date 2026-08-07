// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/handle_table.h"

namespace granit::detail {
namespace {

constexpr std::uint32_t index_bits = 32;
constexpr std::uint32_t generation_bits = 24;
constexpr std::uint32_t type_shift = index_bits + generation_bits;
constexpr granit_handle index_mask = UINT64_C(0xffffffff);
constexpr granit_handle generation_mask = UINT64_C(0x00ffffff);

} // namespace

granit_handle handle_table::insert(
  void* resource,
  resource_type type,
  std::uint32_t domain) {
  if (resource == nullptr || type == resource_type::unknown) {
    return GRANIT_NULL_HANDLE;
  }

  std::uint32_t slot_index = invalid_slot;
  if (free_head_ != invalid_slot) {
    slot_index = free_head_;
    free_head_ = slots_[slot_index].next_free;
  } else {
    // 句柄中的索引以一为起点，因此最多表示 UINT32_MAX 个槽位。
    if (slots_.size() >= std::numeric_limits<std::uint32_t>::max()) {
      return GRANIT_NULL_HANDLE;
    }
    slot_index = static_cast<std::uint32_t>(slots_.size());
    slots_.emplace_back();
  }

  auto& target = slots_[slot_index];
  target.resource = resource;
  target.domain = domain;
  target.type = type;
  target.next_free = invalid_slot;
  ++active_count_;
  return encode(slot_index, target.generation, type);
}

void* handle_table::find(
  granit_handle handle,
  resource_type expected_type,
  std::uint32_t expected_domain) const noexcept {
  const auto* found = validate(handle, expected_type, expected_domain);
  return found == nullptr ? nullptr : found->resource;
}

granit_result handle_table::erase(
  granit_handle handle,
  resource_type expected_type,
  std::uint32_t expected_domain,
  void** resource) noexcept {
  decoded_handle decoded{};
  const auto* found = validate(handle, expected_type, expected_domain, &decoded);
  if (found == nullptr) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }

  auto& target = slots_[decoded.slot_index];
  if (resource != nullptr) {
    *resource = target.resource;
  }
  target.resource = nullptr;
  target.domain = 0;
  target.type = resource_type::unknown;
  target.generation = target.generation == maximum_generation ? 1 : target.generation + 1;
  target.next_free = free_head_;
  free_head_ = decoded.slot_index;
  --active_count_;
  return GRANIT_SUCCESS;
}

granit_handle handle_table::encode(
  std::uint32_t slot_index,
  std::uint32_t generation,
  resource_type type) noexcept {
  const auto encoded_index = static_cast<granit_handle>(slot_index) + 1;
  const auto encoded_generation = static_cast<granit_handle>(generation) << index_bits;
  const auto encoded_type = static_cast<granit_handle>(type) << type_shift;
  return encoded_type | encoded_generation | encoded_index;
}

bool handle_table::decode(granit_handle handle, decoded_handle& decoded) noexcept {
  if (handle == GRANIT_NULL_HANDLE) {
    return false;
  }

  const auto encoded_index = static_cast<std::uint32_t>(handle & index_mask);
  if (encoded_index == 0) {
    return false;
  }

  decoded.slot_index = encoded_index - 1;
  decoded.generation = static_cast<std::uint32_t>((handle >> index_bits) & generation_mask);
  decoded.type = static_cast<resource_type>(handle >> type_shift);
  return decoded.generation != 0 && decoded.type != resource_type::unknown;
}

const handle_table::slot* handle_table::validate(
  granit_handle handle,
  resource_type expected_type,
  std::uint32_t expected_domain,
  decoded_handle* decoded) const noexcept {
  decoded_handle value{};
  if (!decode(handle, value) || value.slot_index >= slots_.size() || value.type != expected_type) {
    return nullptr;
  }

  const auto& candidate = slots_[value.slot_index];
  if (candidate.resource == nullptr || candidate.generation != value.generation ||
      candidate.type != expected_type || candidate.domain != expected_domain) {
    return nullptr;
  }

  if (decoded != nullptr) {
    *decoded = value;
  }
  return &candidate;
}

} // namespace granit::detail
