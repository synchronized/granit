// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/debug_draw_list.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x45);

struct list_state {
  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  std::vector<granit_debug_draw_line> lines;
  std::vector<granit_debug_draw_triangle> triangles;
};
struct slot {
  std::shared_ptr<list_state> state;
  uint32_t generation = 1;
};

std::mutex registry_mutex;
std::vector<slot> registry;

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
std::shared_ptr<list_state> find(granit_renderer renderer, granit_debug_draw_list list) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(list, index, generation))
    return {};
  std::scoped_lock lock{registry_mutex};
  if (index >= registry.size() || registry[index].generation != generation ||
      registry[index].state == nullptr || registry[index].state->renderer != renderer)
    return {};
  return registry[index].state;
}
bool zero(const uint32_t* values, size_t count) {
  for (size_t index = 0; index < count; ++index)
    if (values[index] != 0)
      return false;
  return true;
}
bool valid_vertex(const granit_debug_draw_vertex& vertex) {
  return std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z);
}
bool valid_space(uint32_t value) {
  return value == GRANIT_DEBUG_DRAW_SPACE_WORLD || value == GRANIT_DEBUG_DRAW_SPACE_SCREEN;
}
bool valid_depth(uint32_t value) {
  return value == GRANIT_DEBUG_DRAW_DEPTH_MODE_DISABLED ||
         value == GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST;
}
bool valid_state(uint32_t space, uint32_t depth) {
  return valid_space(space) && valid_depth(depth) &&
         (space != GRANIT_DEBUG_DRAW_SPACE_SCREEN ||
          depth == GRANIT_DEBUG_DRAW_DEPTH_MODE_DISABLED);
}

} // namespace

extern "C" granit_result granit_debug_draw_list_create(granit_renderer renderer,
                                                       const granit_debug_draw_list_desc* desc,
                                                       granit_debug_draw_list* list) {
  if (list == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *list = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr ||
      desc->struct_size < sizeof(granit_debug_draw_list_desc) ||
      !zero(desc->reserved, std::size(desc->reserved)))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  uint64_t cache_size = 0;
  const auto renderer_result =
      granit_renderer_pipeline_cache_export(renderer, nullptr, &cache_size);
  if (renderer_result != GRANIT_SUCCESS)
    return renderer_result;
  try {
    auto state = std::make_shared<list_state>();
    state->renderer = renderer;
    state->lines.reserve(desc->initial_line_capacity);
    state->triangles.reserve(desc->initial_triangle_capacity);
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

extern "C" granit_result granit_debug_draw_list_clear(granit_renderer renderer,
                                                      granit_debug_draw_list list) {
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  state->lines.clear();
  state->triangles.clear();
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_debug_draw_list_append_lines(granit_renderer renderer,
                                                             granit_debug_draw_list list,
                                                             const granit_debug_draw_line* lines,
                                                             uint32_t line_count) {
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (lines == nullptr || line_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint32_t index = 0; index < line_count; ++index) {
    const auto& line = lines[index];
    if (!valid_vertex(line.start) || !valid_vertex(line.end) || !std::isfinite(line.width) ||
        line.width <= 0 || !valid_state(line.space, line.depth_mode) || line.reserved != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    std::scoped_lock lock{state->mutex};
    state->lines.insert(state->lines.end(), lines, lines + line_count);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_debug_draw_list_append_triangles(granit_renderer renderer, granit_debug_draw_list list,
                                        const granit_debug_draw_triangle* triangles,
                                        uint32_t triangle_count) {
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (triangles == nullptr || triangle_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint32_t index = 0; index < triangle_count; ++index) {
    const auto& triangle = triangles[index];
    if (!valid_vertex(triangle.vertices[0]) || !valid_vertex(triangle.vertices[1]) ||
        !valid_vertex(triangle.vertices[2]) || !valid_state(triangle.space, triangle.depth_mode) ||
        !zero(triangle.reserved, std::size(triangle.reserved)))
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    std::scoped_lock lock{state->mutex};
    state->triangles.insert(state->triangles.end(), triangles, triangles + triangle_count);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_debug_draw_list_get_stats(granit_renderer renderer,
                                                          granit_debug_draw_list list,
                                                          granit_debug_draw_list_stats* stats) {
  if (stats == nullptr || stats->struct_size < sizeof(granit_debug_draw_list_stats))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  stats->line_count = static_cast<uint32_t>(state->lines.size());
  stats->triangle_count = static_cast<uint32_t>(state->triangles.size());
  std::fill(std::begin(stats->reserved), std::end(stats->reserved), 0);
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_debug_draw_list_destroy(granit_renderer renderer,
                                                        granit_debug_draw_list list) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(list, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{registry_mutex};
  if (index >= registry.size() || registry[index].generation != generation ||
      registry[index].state == nullptr || registry[index].state->renderer != renderer)
    return GRANIT_ERROR_INVALID_HANDLE;
  registry[index].state.reset();
  registry[index].generation =
      registry[index].generation == generation_mask ? 1 : registry[index].generation + 1;
  return GRANIT_SUCCESS;
}
