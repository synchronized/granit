// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_draw_list.h>

#include "pipeline/text_atlas_access.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x46);

struct text_run {
  uint32_t first_glyph = 0;
  uint32_t glyph_count = 0;
  granit_scissor scissor{};
};

struct list_state {
  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  std::vector<granit_text_glyph_instance> glyphs;
  std::vector<text_run> runs;
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

std::shared_ptr<list_state> find(granit_renderer renderer, granit_text_draw_list list) {
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

bool zero(const uint32_t* values, size_t count) {
  return std::all_of(values, values + count, [](uint32_t value) { return value == 0; });
}

bool valid_scissor(const granit_scissor& scissor) {
  if (scissor.width == 0 || scissor.height == 0) {
    return scissor.x == 0 && scissor.y == 0 && scissor.width == 0 && scissor.height == 0;
  }
  return true;
}

} // namespace

extern "C" granit_result granit_text_draw_list_create(granit_renderer renderer,
                                                      const granit_text_draw_list_desc* desc,
                                                      granit_text_draw_list* list) {
  if (list == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *list = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr ||
      desc->struct_size < GRANIT_TEXT_DRAW_LIST_DESC_VERSION_1_SIZE ||
      !zero(desc->reserved, std::size(desc->reserved))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  uint64_t cache_size = 0;
  const auto renderer_result =
      granit_renderer_pipeline_cache_export(renderer, nullptr, &cache_size);
  if (renderer_result != GRANIT_SUCCESS)
    return renderer_result;
  try {
    auto state = std::make_shared<list_state>();
    state->renderer = renderer;
    state->glyphs.reserve(desc->initial_glyph_capacity);
    state->runs.reserve(desc->initial_run_capacity);
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

extern "C" granit_result granit_text_draw_list_clear(granit_renderer renderer,
                                                     granit_text_draw_list list) {
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  state->glyphs.clear();
  state->runs.clear();
  return GRANIT_SUCCESS;
}

extern "C" granit_result
granit_text_draw_list_append_glyph_run(granit_renderer renderer, granit_text_draw_list list,
                                       const granit_text_glyph_run_desc* run) {
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (run == nullptr || run->struct_size < GRANIT_TEXT_GLYPH_RUN_DESC_VERSION_1_SIZE ||
      run->glyph_count == 0 || run->glyphs == nullptr ||
      !zero(run->reserved, std::size(run->reserved)) || !valid_scissor(run->scissor)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (uint32_t index = 0; index < run->glyph_count; ++index) {
    const auto& glyph = run->glyphs[index];
    if (glyph.font_key == 0 || !std::isfinite(glyph.x) || !std::isfinite(glyph.y) ||
        !zero(glyph.reserved, std::size(glyph.reserved))) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  try {
    std::scoped_lock lock{state->mutex};
    if (state->glyphs.size() > UINT32_MAX - run->glyph_count)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto first = static_cast<uint32_t>(state->glyphs.size());
    state->glyphs.insert(state->glyphs.end(), run->glyphs, run->glyphs + run->glyph_count);
    try {
      state->runs.push_back({first, run->glyph_count, run->scissor});
    } catch (...) {
      state->glyphs.resize(first);
      throw;
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_text_draw_list_get_stats(granit_renderer renderer,
                                                         granit_text_draw_list list,
                                                         granit_text_draw_list_stats* stats) {
  if (stats == nullptr || stats->struct_size < GRANIT_TEXT_DRAW_LIST_STATS_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  stats->glyph_count = static_cast<uint32_t>(state->glyphs.size());
  stats->run_count = static_cast<uint32_t>(state->runs.size());
  std::fill(std::begin(stats->reserved), std::end(stats->reserved), 0);
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_text_draw_list_append_to_canvas(granit_renderer renderer,
                                                                granit_text_draw_list list,
                                                                granit_text_atlas atlas,
                                                                granit_canvas_draw_list canvas) {
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (atlas == GRANIT_NULL_HANDLE || canvas == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::scoped_lock lock{state->mutex};
  for (const auto& run : state->runs) {
    for (uint32_t offset = 0; offset < run.glyph_count; ++offset) {
      const auto& instance = state->glyphs[run.first_glyph + offset];
      granit::pipeline::detail::text_atlas_glyph glyph;
      const auto resolved = granit::pipeline::detail::text_atlas_resolve_glyph(
          renderer, atlas, instance.font_key, instance.glyph_id, glyph);
      if (resolved != GRANIT_SUCCESS)
        return resolved;
      if (glyph.view == GRANIT_NULL_HANDLE)
        continue;
      granit_canvas_rect_desc rect = GRANIT_CANVAS_RECT_DESC_INIT;
      rect.x = instance.x + glyph.bearing_x;
      rect.y = instance.y - glyph.bearing_y;
      rect.width = glyph.width;
      rect.height = glyph.height;
      rect.u0 = glyph.u0;
      rect.v0 = glyph.v0;
      rect.u1 = glyph.u1;
      rect.v1 = glyph.v1;
      rect.color = instance.color;
      rect.state.texture = glyph.view;
      rect.state.sampler = glyph.sampler;
      rect.state.scissor = run.scissor;
      const auto appended = granit_canvas_draw_list_append_rect(renderer, canvas, &rect);
      if (appended != GRANIT_SUCCESS)
        return appended;
    }
  }
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_text_draw_list_destroy(granit_renderer renderer,
                                                       granit_text_draw_list list) {
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
