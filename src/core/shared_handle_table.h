// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_SHARED_HANDLE_TABLE_H_
#define GRANIT_CORE_SHARED_HANDLE_TABLE_H_

#include "core/handle_encoding.h"

#include <limits>
#include <memory>
#include <mutex>
#include <vector>

#include <granit/core/result.h>

namespace granit::detail {

/** 保存共享对象的不透明句柄表；查找返回 shared_ptr，使并发查询不受销毁影响。 */
template <typename T, handle_type Type> class shared_handle_table {
public:
  shared_handle_table() = default;

  shared_handle_table(const shared_handle_table&) = delete;
  shared_handle_table& operator=(const shared_handle_table&) = delete;
  shared_handle_table(shared_handle_table&&) = delete;
  shared_handle_table& operator=(shared_handle_table&&) = delete;

  [[nodiscard]] granit_handle insert(std::shared_ptr<const T> value) {
    if (!value)
      return GRANIT_NULL_HANDLE;
    std::scoped_lock lock{mutex_};
    std::uint32_t slot_index{};
    if (free_head_ != invalid_slot) {
      slot_index = free_head_;
      free_head_ = slots_[slot_index].next_free;
    } else {
      if (slots_.size() >= std::numeric_limits<std::uint32_t>::max())
        return GRANIT_NULL_HANDLE;
      slot_index = static_cast<std::uint32_t>(slots_.size());
      slots_.emplace_back();
    }
    auto& target = slots_[slot_index];
    target.value = std::move(value);
    target.next_free = invalid_slot;
    return encode_handle(slot_index, target.generation, Type);
  }

  [[nodiscard]] std::shared_ptr<const T> find(granit_handle handle) const noexcept {
    decoded_handle decoded{};
    if (!decode_handle(handle, decoded) || decoded.type != Type)
      return {};
    std::scoped_lock lock{mutex_};
    if (decoded.slot_index >= slots_.size())
      return {};
    const auto& candidate = slots_[decoded.slot_index];
    if (!candidate.value || candidate.generation != decoded.generation)
      return {};
    return candidate.value;
  }

  [[nodiscard]] granit_result erase(granit_handle handle) noexcept {
    decoded_handle decoded{};
    if (!decode_handle(handle, decoded) || decoded.type != Type)
      return GRANIT_ERROR_INVALID_HANDLE;
    std::scoped_lock lock{mutex_};
    if (decoded.slot_index >= slots_.size())
      return GRANIT_ERROR_INVALID_HANDLE;
    auto& candidate = slots_[decoded.slot_index];
    if (!candidate.value || candidate.generation != decoded.generation)
      return GRANIT_ERROR_INVALID_HANDLE;
    candidate.value.reset();
    candidate.generation = next_handle_generation(candidate.generation);
    candidate.next_free = free_head_;
    free_head_ = decoded.slot_index;
    return GRANIT_SUCCESS;
  }

private:
  static constexpr std::uint32_t invalid_slot = std::numeric_limits<std::uint32_t>::max();

  struct slot {
    std::shared_ptr<const T> value;
    std::uint32_t generation{1};
    std::uint32_t next_free{invalid_slot};
  };

  mutable std::mutex mutex_;
  std::vector<slot> slots_;
  std::uint32_t free_head_{invalid_slot};
};

} // namespace granit::detail

#endif
