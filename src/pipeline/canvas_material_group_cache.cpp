// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/canvas_material_group_cache.h"

#include "pipeline/material_access.h"

#include <algorithm>
#include <new>

namespace granit::pipeline::detail {

canvas_material_group_cache::~canvas_material_group_cache() { static_cast<void>(reset()); }

granit_result
canvas_material_group_cache::acquire(granit_renderer renderer, granit_material material,
                                     granit_bind_group_layout layout, granit_texture_view texture,
                                     granit_sampler sampler, granit_bind_group& group) {
  group = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || material == GRANIT_NULL_HANDLE ||
      layout == GRANIT_NULL_HANDLE || texture == GRANIT_NULL_HANDLE ||
      sampler == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer_ != renderer || material_ != material || layout_ != layout) {
    const auto result = reset();
    if (result != GRANIT_SUCCESS)
      return result;
    renderer_ = renderer;
    material_ = material;
    layout_ = layout;
  }

  ++use_serial_;
  const auto found = std::ranges::find_if(entries_, [&](const auto& value) {
    return value.texture == texture && value.sampler == sampler;
  });
  if (found != entries_.end()) {
    found->last_use = use_serial_;
    group = found->group;
    return GRANIT_SUCCESS;
  }

  granit_bind_group replacement = GRANIT_NULL_HANDLE;
  const auto result =
      create_canvas_material_group(renderer, material, texture, sampler, replacement);
  if (result != GRANIT_SUCCESS)
    return result;
  try {
    // 本帧准备期间允许暂时超过持久容量，避免尚未绑定的组被提前淘汰。
    entries_.push_back({texture, sampler, replacement, use_serial_});
  } catch (const std::bad_alloc&) {
    static_cast<void>(granit_bind_group_destroy(renderer_, replacement));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  group = replacement;
  return GRANIT_SUCCESS;
}

granit_result canvas_material_group_cache::trim() noexcept {
  granit_result first_error = GRANIT_SUCCESS;
  if (entries_.size() <= capacity)
    return first_error;
  std::ranges::sort(entries_, {}, &entry::last_use);
  const auto remove_count = entries_.size() - capacity;
  for (std::size_t index = 0; index < remove_count; ++index) {
    const auto result = granit_bind_group_destroy(renderer_, entries_[index].group);
    if (first_error == GRANIT_SUCCESS)
      first_error = result;
  }
  entries_.erase(entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>(remove_count));
  return first_error;
}

granit_result canvas_material_group_cache::reset() noexcept {
  granit_result first_error = GRANIT_SUCCESS;
  if (renderer_ != GRANIT_NULL_HANDLE) {
    for (const auto& value : entries_) {
      const auto result = granit_bind_group_destroy(renderer_, value.group);
      if (first_error == GRANIT_SUCCESS)
        first_error = result;
    }
  }
  entries_.clear();
  renderer_ = GRANIT_NULL_HANDLE;
  material_ = GRANIT_NULL_HANDLE;
  layout_ = GRANIT_NULL_HANDLE;
  use_serial_ = 0;
  return first_error;
}

} // namespace granit::pipeline::detail
