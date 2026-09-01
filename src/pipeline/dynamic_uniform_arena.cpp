// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/dynamic_uniform_arena.h"

#include <granit/renderer/renderer.h>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <new>

namespace granit::pipeline::detail {
namespace {

bool add_overflows(std::uint64_t left, std::uint64_t right) noexcept {
  return right > std::numeric_limits<std::uint64_t>::max() - left;
}

} // namespace

uniform_arena_error
dynamic_uniform_arena_plan::initialize(std::uint64_t alignment, std::uint64_t max_binding_size,
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

granit_result dynamic_uniform_arena::initialize(granit_renderer renderer) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || renderer_ != GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  const auto result = granit_renderer_get_limits(renderer, &limits);
  if (result != GRANIT_SUCCESS)
    return result;
  if (limits.uniform_buffer_offset_alignment == 0 || limits.max_uniform_buffer_binding_size == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  renderer_ = renderer;
  alignment_ = limits.uniform_buffer_offset_alignment;
  max_binding_size_ = limits.max_uniform_buffer_binding_size;
  return GRANIT_SUCCESS;
}

granit_result dynamic_uniform_arena::begin_frame(std::uint32_t frame_slot,
                                                 std::uint32_t frame_slot_count) noexcept {
  if (renderer_ == GRANIT_NULL_HANDLE || frame_slot_count == 0 || frame_slot >= frame_slot_count)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    if (slots_.size() < frame_slot_count) {
      const auto previous_size = slots_.size();
      slots_.resize(frame_slot_count);
      for (auto index = previous_size; index < slots_.size(); ++index) {
        if (slots_[index].plan.initialize(alignment_, max_binding_size_, 64 * 1024) !=
            uniform_arena_error::none) {
          return GRANIT_ERROR_INTERNAL;
        }
      }
    }
    current_slot_ = &slots_[frame_slot];
    current_slot_->plan.rewind();
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result dynamic_uniform_arena::ensure_buffer(frame_slot_state& slot) noexcept {
  const auto required_capacity = slot.plan.capacity();
  if (slot.buffer.valid() && slot.buffer_capacity >= required_capacity)
    return GRANIT_SUCCESS;
  granit::buffer replacement;
  const auto result =
      replacement.initialize(renderer_, {.size = required_capacity,
                                         .usage = granit::buffer_usage::uniform |
                                                  granit::buffer_usage::transfer_destination,
                                         .location = granit::memory_location::upload});
  if (granit::failed(result))
    return static_cast<granit_result>(result);
  slot.groups.clear();
  slot.buffer = std::move(replacement);
  slot.buffer_capacity = required_capacity;
  return GRANIT_SUCCESS;
}

granit_result dynamic_uniform_arena::acquire_groups(frame_slot_state& slot,
                                                    const material_draw_state& material,
                                                    group_pair*& output) noexcept {
  const auto found = std::ranges::find_if(slot.groups, [&](const group_pair& groups) {
    return groups.frame_layout == material.frame_layout &&
           groups.object_layout == material.object_layout;
  });
  if (found != slot.groups.end()) {
    output = &*found;
    return GRANIT_SUCCESS;
  }
  try {
    group_pair candidate;
    candidate.frame_layout = material.frame_layout;
    candidate.object_layout = material.object_layout;
    const std::array frame_entry{
        granit::bind_group_entry{.binding = 0,
                                 .resource = slot.buffer.native_handle(),
                                 .offset = 0,
                                 .size = sizeof(granit::material::pbr_frame_constants)}};
    auto result = candidate.frame_group.initialize(renderer_, material.frame_layout, frame_entry);
    const std::array object_entry{
        granit::bind_group_entry{.binding = 0,
                                 .resource = slot.buffer.native_handle(),
                                 .offset = 0,
                                 .size = sizeof(granit::material::pbr_object_constants)}};
    if (granit::succeeded(result))
      result = candidate.object_group.initialize(renderer_, material.object_layout, object_entry);
    if (granit::failed(result))
      return static_cast<granit_result>(result);
    slot.groups.push_back(std::move(candidate));
    output = &slot.groups.back();
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result dynamic_uniform_arena::prepare(const material_draw_state& material,
                                             std::span<const std::byte> frame,
                                             std::span<const std::byte> object,
                                             dynamic_uniform_binding& output) noexcept {
  if (current_slot_ == nullptr || material.frame_layout == GRANIT_NULL_HANDLE ||
      material.object_layout == GRANIT_NULL_HANDLE ||
      frame.size() != sizeof(granit::material::pbr_frame_constants) ||
      object.size() != sizeof(granit::material::pbr_object_constants))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  uniform_arena_allocation frame_allocation;
  uniform_arena_allocation object_allocation;
  auto error = current_slot_->plan.allocate(frame.size(), frame_allocation);
  if (error == uniform_arena_error::none)
    error = current_slot_->plan.allocate(object.size(), object_allocation);
  if (error != uniform_arena_error::none ||
      frame_allocation.offset > std::numeric_limits<std::uint32_t>::max() ||
      object_allocation.offset > std::numeric_limits<std::uint32_t>::max()) {
    return error == uniform_arena_error::binding_too_large ? GRANIT_ERROR_UNSUPPORTED
                                                           : GRANIT_ERROR_OUT_OF_MEMORY;
  }
  auto result = ensure_buffer(*current_slot_);
  if (result == GRANIT_SUCCESS)
    result =
        static_cast<granit_result>(current_slot_->buffer.write(frame_allocation.offset, frame));
  if (result == GRANIT_SUCCESS) {
    result =
        static_cast<granit_result>(current_slot_->buffer.write(object_allocation.offset, object));
  }
  group_pair* groups = nullptr;
  if (result == GRANIT_SUCCESS)
    result = acquire_groups(*current_slot_, material, groups);
  if (result == GRANIT_SUCCESS) {
    output = {.frame_group = groups->frame_group.native_handle(),
              .object_group = groups->object_group.native_handle(),
              .frame_offset = static_cast<std::uint32_t>(frame_allocation.offset),
              .object_offset = static_cast<std::uint32_t>(object_allocation.offset)};
  }
  return result;
}

granit_result dynamic_uniform_arena::reset() noexcept {
  slots_.clear();
  current_slot_ = nullptr;
  renderer_ = GRANIT_NULL_HANDLE;
  return GRANIT_SUCCESS;
}

} // namespace granit::pipeline::detail
