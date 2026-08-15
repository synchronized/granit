// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/debug_draw_list.h>

#include <granit/renderer/sampler.h>
#include <granit/renderer/texture.h>

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
  ~list_state() {
    if (sampler != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_sampler_destroy(renderer, sampler));
    if (white_view != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_texture_view_destroy(renderer, white_view));
    if (white_texture != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_texture_destroy(renderer, white_texture));
  }
  struct command {
    bool is_line = false;
    granit_debug_draw_line line{};
    granit_debug_draw_triangle triangle{};
  };
  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  std::vector<command> commands;
  granit_texture white_texture = GRANIT_NULL_HANDLE;
  granit_texture_view white_view = GRANIT_NULL_HANDLE;
  granit_sampler sampler = GRANIT_NULL_HANDLE;
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

granit_result ensure_white_resources(list_state& state) {
  if (state.sampler != GRANIT_NULL_HANDLE)
    return GRANIT_SUCCESS;
  auto result = GRANIT_SUCCESS;
  if (state.white_texture == GRANIT_NULL_HANDLE) {
    granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
    texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
    texture_desc.usage =
        GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
    result = granit_texture_create_with_default_view(state.renderer, &texture_desc,
                                                     &state.white_texture, &state.white_view);
    constexpr uint32_t white = UINT32_C(0xffffffff);
    const granit_texture_data_layout layout{};
    const granit_texture_write_region region{0, 0, 1, GRANIT_TEXTURE_ASPECT_COLOR_BIT, 0, 0, 0,
                                             1, 1, 1};
    if (result == GRANIT_SUCCESS)
      result = granit_texture_write(state.renderer, state.white_texture, &white, sizeof(white),
                                    &layout, &region);
  }
  granit_sampler_desc sampler_desc = GRANIT_SAMPLER_DESC_INIT;
  sampler_desc.mag_filter = GRANIT_FILTER_NEAREST;
  sampler_desc.min_filter = GRANIT_FILTER_NEAREST;
  if (result == GRANIT_SUCCESS)
    result = granit_sampler_create(state.renderer, &sampler_desc, &state.sampler);
  if (result != GRANIT_SUCCESS) {
    if (state.sampler != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_sampler_destroy(state.renderer, state.sampler));
    if (state.white_view != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_texture_view_destroy(state.renderer, state.white_view));
    if (state.white_texture != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_texture_destroy(state.renderer, state.white_texture));
    state.sampler = GRANIT_NULL_HANDLE;
    state.white_view = GRANIT_NULL_HANDLE;
    state.white_texture = GRANIT_NULL_HANDLE;
  }
  return result;
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
    state->commands.reserve(static_cast<size_t>(desc->initial_line_capacity) +
                            desc->initial_triangle_capacity);
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
  state->commands.clear();
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
    const auto dx = line.end.x - line.start.x;
    const auto dy = line.end.y - line.start.y;
    const auto dz = line.end.z - line.start.z;
    if (!valid_vertex(line.start) || !valid_vertex(line.end) || !std::isfinite(line.width) ||
        line.width <= 0 || !valid_state(line.space, line.depth_mode) || line.reserved != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if ((line.space == GRANIT_DEBUG_DRAW_SPACE_SCREEN && dx == 0 && dy == 0) ||
        (line.space == GRANIT_DEBUG_DRAW_SPACE_WORLD && dx == 0 && dy == 0 && dz == 0))
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    std::scoped_lock lock{state->mutex};
    state->commands.reserve(state->commands.size() + line_count);
    for (uint32_t index = 0; index < line_count; ++index)
      state->commands.push_back({.is_line = true, .line = lines[index], .triangle = {}});
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
    state->commands.reserve(state->commands.size() + triangle_count);
    for (uint32_t index = 0; index < triangle_count; ++index)
      state->commands.push_back({.is_line = false, .line = {}, .triangle = triangles[index]});
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
  stats->line_count = 0;
  stats->triangle_count = 0;
  for (const auto& command : state->commands) {
    if (command.is_line)
      ++stats->line_count;
    else
      ++stats->triangle_count;
  }
  std::fill(std::begin(stats->reserved), std::end(stats->reserved), 0);
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_debug_draw_list_append_screen_to_canvas(
    granit_renderer renderer, granit_debug_draw_list list, granit_canvas_draw_list canvas) {
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  granit_canvas_draw_list_stats canvas_stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  const auto canvas_result = granit_canvas_draw_list_get_stats(renderer, canvas, &canvas_stats);
  if (canvas_result != GRANIT_SUCCESS)
    return canvas_result;
  try {
    std::scoped_lock lock{state->mutex};
    std::vector<granit_canvas_vertex> vertices;
    std::vector<uint32_t> indices;
    for (const auto& command : state->commands) {
      if (command.is_line && command.line.space == GRANIT_DEBUG_DRAW_SPACE_SCREEN) {
        const auto& line = command.line;
        const auto dx = line.end.x - line.start.x;
        const auto dy = line.end.y - line.start.y;
        const auto scale = line.width * 0.5F / std::sqrt(dx * dx + dy * dy);
        const auto px = -dy * scale;
        const auto py = dx * scale;
        const auto first = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(),
                        {{line.start.x + px, line.start.y + py, 0, 0, line.start.color},
                         {line.start.x - px, line.start.y - py, 0, 0, line.start.color},
                         {line.end.x + px, line.end.y + py, 0, 0, line.end.color},
                         {line.end.x - px, line.end.y - py, 0, 0, line.end.color}});
        indices.insert(indices.end(),
                       {first, first + 1, first + 2, first + 2, first + 1, first + 3});
      } else if (!command.is_line && command.triangle.space == GRANIT_DEBUG_DRAW_SPACE_SCREEN) {
        const auto first = static_cast<uint32_t>(vertices.size());
        for (const auto& vertex : command.triangle.vertices)
          vertices.push_back({vertex.x, vertex.y, 0, 0, vertex.color});
        indices.insert(indices.end(), {first, first + 1, first + 2});
      }
    }
    if (vertices.empty())
      return GRANIT_SUCCESS;
    auto result = ensure_white_resources(*state);
    if (result != GRANIT_SUCCESS)
      return result;
    const granit_canvas_draw_state draw_state{state->white_view, state->sampler, {0, 0, 0, 0}};
    return granit_canvas_draw_list_append(renderer, canvas, vertices.data(),
                                          static_cast<uint32_t>(vertices.size()), indices.data(),
                                          static_cast<uint32_t>(indices.size()), &draw_state);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
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
