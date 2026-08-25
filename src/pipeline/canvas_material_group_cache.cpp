// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/canvas_material_group_cache.h"

#include "pipeline/material_access.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace granit::pipeline::detail {

struct canvas_material_group_cache::impl {
  struct entry {
    granit_texture_view texture = GRANIT_NULL_HANDLE;
    granit_sampler sampler = GRANIT_NULL_HANDLE;
    granit_bind_group group = GRANIT_NULL_HANDLE;
    std::uint64_t last_use = 0;
  };

  static constexpr std::size_t capacity = 64;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_material material = GRANIT_NULL_HANDLE;
  granit_bind_group_layout layout = GRANIT_NULL_HANDLE;
  std::uint64_t use_serial = 0;
  std::vector<entry> entries;
};

canvas_material_group_cache::canvas_material_group_cache() : state_(std::make_unique<impl>()) {}

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
  if (state_->renderer != renderer || state_->material != material || state_->layout != layout) {
    const auto result = reset();
    if (result != GRANIT_SUCCESS)
      return result;
    state_->renderer = renderer;
    state_->material = material;
    state_->layout = layout;
  }

  ++state_->use_serial;
  const auto found = std::ranges::find_if(state_->entries, [&](const auto& value) {
    return value.texture == texture && value.sampler == sampler;
  });
  if (found != state_->entries.end()) {
    found->last_use = state_->use_serial;
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
    state_->entries.push_back({texture, sampler, replacement, state_->use_serial});
  } catch (const std::bad_alloc&) {
    static_cast<void>(granit_bind_group_destroy(state_->renderer, replacement));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  group = replacement;
  return GRANIT_SUCCESS;
}

granit_result canvas_material_group_cache::trim() noexcept {
  granit_result first_error = GRANIT_SUCCESS;
  if (state_->entries.size() <= impl::capacity)
    return first_error;
  std::ranges::sort(state_->entries, {}, &impl::entry::last_use);
  const auto remove_count = state_->entries.size() - impl::capacity;
  for (std::size_t index = 0; index < remove_count; ++index) {
    const auto result = granit_bind_group_destroy(state_->renderer, state_->entries[index].group);
    if (first_error == GRANIT_SUCCESS)
      first_error = result;
  }
  state_->entries.erase(state_->entries.begin(),
                        state_->entries.begin() + static_cast<std::ptrdiff_t>(remove_count));
  return first_error;
}

granit_result canvas_material_group_cache::reset() noexcept {
  granit_result first_error = GRANIT_SUCCESS;
  if (state_->renderer != GRANIT_NULL_HANDLE) {
    for (const auto& value : state_->entries) {
      const auto result = granit_bind_group_destroy(state_->renderer, value.group);
      if (first_error == GRANIT_SUCCESS)
        first_error = result;
    }
  }
  state_->entries.clear();
  state_->renderer = GRANIT_NULL_HANDLE;
  state_->material = GRANIT_NULL_HANDLE;
  state_->layout = GRANIT_NULL_HANDLE;
  state_->use_serial = 0;
  return first_error;
}

} // namespace granit::pipeline::detail
