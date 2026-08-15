// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/ui_draw_list.h>

#include "pipeline/ui_draw_list.h"

#include <array>
#include <cmath>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x44);

struct ui_draw_list_state {
  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit::pipeline::detail::ui_draw_list list;
};

struct ui_draw_list_slot {
  std::shared_ptr<ui_draw_list_state> state;
  uint32_t generation = 1;
};

std::mutex registry_mutex;
std::vector<ui_draw_list_slot> registry;

granit_handle encode(size_t index, uint32_t generation) {
  return (type_value << 56) | (static_cast<uint64_t>(generation) << 32) |
         (static_cast<uint64_t>(index) + 1);
}

bool decode(granit_handle handle, size_t& index, uint32_t& generation) {
  if ((handle >> 56) != type_value || (handle & index_mask) == 0)
    return false;
  index = static_cast<size_t>((handle & index_mask) - 1);
  generation = static_cast<uint32_t>((handle >> 32) & generation_mask);
  return generation != 0;
}

std::shared_ptr<ui_draw_list_state> find_list(granit_renderer renderer, granit_ui_draw_list list) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(list, index, generation))
    return {};
  std::scoped_lock lock{registry_mutex};
  if (index >= registry.size() || registry[index].generation != generation ||
      registry[index].state == nullptr || registry[index].state->renderer != renderer) {
    return {};
  }
  return registry[index].state;
}

bool reserved_is_zero(const uint32_t* values, size_t count) {
  for (size_t index = 0; index < count; ++index) {
    if (values[index] != 0)
      return false;
  }
  return true;
}

bool valid_state(const granit_ui_draw_state& state) {
  return state.texture != GRANIT_NULL_HANDLE && state.sampler != GRANIT_NULL_HANDLE;
}

granit::pipeline::detail::ui_draw_state convert_state(const granit_ui_draw_state& state) {
  return {.texture = state.texture,
          .sampler = state.sampler,
          .scissor = state.scissor,
          .layer = state.layer};
}

} // namespace

extern "C" granit_result granit_ui_draw_list_create(granit_renderer renderer,
                                                    const granit_ui_draw_list_desc* desc,
                                                    granit_ui_draw_list* list) {
  if (list == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *list = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr ||
      desc->struct_size < sizeof(granit_ui_draw_list_desc) ||
      !reserved_is_zero(desc->reserved, std::size(desc->reserved))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  uint64_t pipeline_cache_size = 0;
  const auto renderer_result =
      granit_renderer_pipeline_cache_export(renderer, nullptr, &pipeline_cache_size);
  if (renderer_result != GRANIT_SUCCESS)
    return renderer_result;
  try {
    auto state = std::make_shared<ui_draw_list_state>();
    state->renderer = renderer;
    const auto reserve_result = state->list.reserve(
        desc->initial_vertex_capacity, desc->initial_index_capacity, desc->initial_item_capacity);
    if (reserve_result != GRANIT_SUCCESS)
      return reserve_result;
    std::scoped_lock lock{registry_mutex};
    size_t index = 0;
    while (index < registry.size() && registry[index].state != nullptr)
      ++index;
    if (index == registry.size())
      registry.emplace_back();
    registry[index].state = std::move(state);
    *list = encode(index, registry[index].generation);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_ui_draw_list_reset(granit_renderer renderer,
                                                   granit_ui_draw_list list) {
  const auto state = find_list(renderer, list);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  state->list.clear();
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_ui_draw_list_append(granit_renderer renderer,
                                                    granit_ui_draw_list list,
                                                    const granit_ui_vertex* vertices,
                                                    uint32_t vertex_count, const uint32_t* indices,
                                                    uint32_t index_count,
                                                    const granit_ui_draw_state* draw_state) {
  const auto state = find_list(renderer, list);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (vertices == nullptr || vertex_count == 0 || indices == nullptr || index_count == 0 ||
      index_count % 3 != 0 || draw_state == nullptr || !valid_state(*draw_state)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (uint32_t index = 0; index < vertex_count; ++index) {
    const auto& vertex = vertices[index];
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.u) ||
        !std::isfinite(vertex.v)) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  std::scoped_lock lock{state->mutex};
  return state->list.append(std::span{vertices, vertex_count}, std::span{indices, index_count},
                            convert_state(*draw_state));
}

extern "C" granit_result granit_ui_draw_list_append_rect(granit_renderer renderer,
                                                         granit_ui_draw_list list,
                                                         const granit_ui_rect_desc* desc) {
  if (desc == nullptr || desc->struct_size < sizeof(granit_ui_rect_desc) ||
      !reserved_is_zero(desc->reserved, std::size(desc->reserved)) || !std::isfinite(desc->x) ||
      !std::isfinite(desc->y) || !std::isfinite(desc->width) || !std::isfinite(desc->height) ||
      !std::isfinite(desc->u0) || !std::isfinite(desc->v0) || !std::isfinite(desc->u1) ||
      !std::isfinite(desc->v1) || desc->width <= 0.0F || desc->height <= 0.0F) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::array vertices{
      granit_ui_vertex{desc->x, desc->y, desc->u0, desc->v0, desc->color},
      granit_ui_vertex{desc->x + desc->width, desc->y, desc->u1, desc->v0, desc->color},
      granit_ui_vertex{desc->x + desc->width, desc->y + desc->height, desc->u1, desc->v1,
                       desc->color},
      granit_ui_vertex{desc->x, desc->y + desc->height, desc->u0, desc->v1, desc->color}};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 2, 3, 0};
  return granit_ui_draw_list_append(renderer, list, vertices.data(),
                                    static_cast<uint32_t>(vertices.size()), indices.data(),
                                    static_cast<uint32_t>(indices.size()), &desc->state);
}

extern "C" granit_result granit_ui_draw_list_get_stats(granit_renderer renderer,
                                                       granit_ui_draw_list list,
                                                       granit_ui_draw_list_stats* stats) {
  if (stats == nullptr || stats->struct_size < sizeof(granit_ui_draw_list_stats))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto state = find_list(renderer, list);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  const auto batches = state->list.batches();
  stats->vertex_count = static_cast<uint32_t>(state->list.vertices().size());
  stats->index_count = static_cast<uint32_t>(state->list.indices().size());
  stats->item_count = static_cast<uint32_t>(state->list.items().size());
  stats->batch_count = static_cast<uint32_t>(batches.size());
  stats->reserved[0] = 0;
  stats->reserved[1] = 0;
  stats->reserved[2] = 0;
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_ui_draw_list_destroy(granit_renderer renderer,
                                                     granit_ui_draw_list list) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(list, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{registry_mutex};
  if (index >= registry.size() || registry[index].generation != generation ||
      registry[index].state == nullptr || registry[index].state->renderer != renderer) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  registry[index].state.reset();
  registry[index].generation =
      registry[index].generation == generation_mask ? 1 : registry[index].generation + 1;
  return GRANIT_SUCCESS;
}
