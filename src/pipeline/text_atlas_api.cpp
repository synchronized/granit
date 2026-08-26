// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_atlas.h>

#include "pipeline/text_atlas_access.h"

#include <granit/renderer/sampler.h>
#include <granit/renderer/texture.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x47);

struct glyph_key {
  uint64_t font_key = 0;
  uint32_t glyph_id = 0;
  bool operator==(const glyph_key&) const = default;
};

struct glyph_key_hash {
  size_t operator()(const glyph_key& key) const noexcept {
    const auto mixed = key.font_key ^ (static_cast<uint64_t>(key.glyph_id) << 32) ^ key.glyph_id;
    return std::hash<uint64_t>{}(mixed);
  }
};

struct glyph_entry {
  uint32_t page = UINT32_MAX;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  float bearing_x = 0;
  float bearing_y = 0;
};

struct atlas_page {
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  uint32_t cursor_x = 0;
  uint32_t cursor_y = 0;
  uint32_t row_height = 0;
};

struct atlas_state {
  ~atlas_state() {
    for (auto& page : pages) {
      if (page.view != GRANIT_NULL_HANDLE)
        static_cast<void>(granit_texture_view_destroy(renderer, page.view));
      if (page.texture != GRANIT_NULL_HANDLE)
        static_cast<void>(granit_texture_destroy(renderer, page.texture));
    }
    if (sampler != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_sampler_destroy(renderer, sampler));
  }

  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  uint32_t page_width = 0;
  uint32_t page_height = 0;
  uint32_t max_pages = 0;
  uint32_t padding = 0;
  granit_sampler sampler = GRANIT_NULL_HANDLE;
  std::vector<atlas_page> pages;
  std::unordered_map<glyph_key, glyph_entry, glyph_key_hash> glyphs;
};

struct slot {
  std::shared_ptr<atlas_state> state;
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

std::shared_ptr<atlas_state> find(granit_renderer renderer, granit_text_atlas atlas) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(atlas, index, generation))
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

granit_result create_page(atlas_state& state, atlas_page& page) {
  granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
  desc.format = GRANIT_TEXTURE_FORMAT_R8_UNORM;
  desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
  desc.width = state.page_width;
  desc.height = state.page_height;
  auto result = granit_texture_create(state.renderer, &desc, &page.texture);
  if (result != GRANIT_SUCCESS)
    return result;
  granit_texture_view_desc view_desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
  view_desc.components.red = GRANIT_COMPONENT_SWIZZLE_ONE;
  view_desc.components.green = GRANIT_COMPONENT_SWIZZLE_ONE;
  view_desc.components.blue = GRANIT_COMPONENT_SWIZZLE_ONE;
  view_desc.components.alpha = GRANIT_COMPONENT_SWIZZLE_RED;
  result = granit_texture_view_create(state.renderer, page.texture, &view_desc, &page.view);
  if (result != GRANIT_SUCCESS)
    static_cast<void>(granit_texture_destroy(state.renderer, page.texture));
  if (result != GRANIT_SUCCESS) {
    page.texture = GRANIT_NULL_HANDLE;
    return result;
  }
  try {
    std::vector<uint8_t> empty(static_cast<size_t>(state.page_width) * state.page_height, 0);
    const granit_texture_data_layout layout{0, state.page_width, state.page_height};
    const granit_texture_write_region region{
        0, 0, 1, GRANIT_TEXTURE_ASPECT_COLOR_BIT, 0, 0, 0, state.page_width, state.page_height, 1};
    result = granit_texture_write(state.renderer, page.texture, empty.data(), empty.size(), &layout,
                                  &region);
  } catch (const std::bad_alloc&) {
    result = GRANIT_ERROR_OUT_OF_MEMORY;
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_texture_view_destroy(state.renderer, page.view));
    static_cast<void>(granit_texture_destroy(state.renderer, page.texture));
    page = {};
    return result;
  }
  page.cursor_x = state.padding;
  page.cursor_y = state.padding;
  return GRANIT_SUCCESS;
}

bool find_position(const atlas_state& state, const atlas_page& page, uint32_t width,
                   uint32_t height, uint32_t& x, uint32_t& y, uint32_t& next_x, uint32_t& next_y,
                   uint32_t& next_row_height) {
  x = page.cursor_x;
  y = page.cursor_y;
  auto row_height = page.row_height;
  if (x + width + state.padding > state.page_width) {
    x = state.padding;
    y += row_height + state.padding;
    row_height = 0;
  }
  if (y + height + state.padding > state.page_height)
    return false;
  next_x = x + width + state.padding;
  next_y = y;
  next_row_height = std::max(row_height, height);
  return true;
}

granit_result write_glyph(const atlas_state& state, const atlas_page& page,
                          const granit_text_glyph_bitmap_desc& glyph, uint32_t x, uint32_t y) {
  const auto bytes_per_row = glyph.bytes_per_row == 0 ? glyph.width : glyph.bytes_per_row;
  const granit_texture_data_layout layout{0, bytes_per_row, glyph.height};
  const granit_texture_write_region region{
      0, 0, 1, GRANIT_TEXTURE_ASPECT_COLOR_BIT, x, y, 0, glyph.width, glyph.height, 1};
  return granit_texture_write(state.renderer, page.texture, glyph.bitmap, glyph.bitmap_size,
                              &layout, &region);
}

} // namespace

extern "C" granit_result granit_text_atlas_create(granit_renderer renderer,
                                                  const granit_text_atlas_desc* desc,
                                                  granit_text_atlas* atlas) {
  if (atlas == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *atlas = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_TEXT_ATLAS_DESC_VERSION_1_SIZE ||
      !zero(desc->reserved, std::size(desc->reserved)) || desc->page_width == 0 ||
      desc->page_height == 0 || desc->page_width > 4096 || desc->page_height > 4096 ||
      desc->max_pages == 0 || desc->max_pages > 256 || desc->padding * 2 >= desc->page_width ||
      desc->padding * 2 >= desc->page_height) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  uint64_t cache_size = 0;
  const auto renderer_result =
      granit_renderer_pipeline_cache_export(renderer, nullptr, &cache_size);
  if (renderer_result != GRANIT_SUCCESS)
    return renderer_result;
  try {
    auto state = std::make_shared<atlas_state>();
    state->renderer = renderer;
    state->page_width = desc->page_width;
    state->page_height = desc->page_height;
    state->max_pages = desc->max_pages;
    state->padding = desc->padding;
    state->pages.reserve(desc->max_pages);
    granit_sampler_desc sampler_desc = GRANIT_SAMPLER_DESC_INIT;
    sampler_desc.address_mode_u = GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_mode_v = GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_mode_w = GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE;
    const auto sampler_result = granit_sampler_create(renderer, &sampler_desc, &state->sampler);
    if (sampler_result != GRANIT_SUCCESS)
      return sampler_result;
    std::scoped_lock lock{registry_mutex};
    size_t index = 0;
    while (index < registry.size() && registry[index].state != nullptr)
      ++index;
    if (index == registry.size())
      registry.emplace_back();
    registry[index].state = std::move(state);
    *atlas = encode(index, registry[index].generation);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result granit::pipeline::detail::text_atlas_resolve_glyph(granit_renderer renderer,
                                                                 granit_text_atlas atlas,
                                                                 uint64_t font_key,
                                                                 uint32_t glyph_id,
                                                                 text_atlas_glyph& glyph) noexcept {
  const auto state = find(renderer, atlas);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  const auto found = state->glyphs.find({font_key, glyph_id});
  if (found == state->glyphs.end())
    return GRANIT_ERROR_NOT_READY;
  const auto& entry = found->second;
  glyph = {.sampler = state->sampler,
           .width = static_cast<float>(entry.width),
           .height = static_cast<float>(entry.height),
           .bearing_x = entry.bearing_x,
           .bearing_y = entry.bearing_y};
  if (entry.page == UINT32_MAX)
    return GRANIT_SUCCESS;
  const auto& page = state->pages[entry.page];
  glyph.view = page.view;
  glyph.u0 = static_cast<float>(entry.x) / static_cast<float>(state->page_width);
  glyph.v0 = static_cast<float>(entry.y) / static_cast<float>(state->page_height);
  glyph.u1 = static_cast<float>(entry.x + entry.width) / static_cast<float>(state->page_width);
  glyph.v1 = static_cast<float>(entry.y + entry.height) / static_cast<float>(state->page_height);
  return GRANIT_SUCCESS;
}

extern "C" granit_result
granit_text_atlas_upload_glyph(granit_renderer renderer, granit_text_atlas atlas,
                               const granit_text_glyph_bitmap_desc* glyph) {
  const auto state = find(renderer, atlas);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (glyph == nullptr || glyph->struct_size < GRANIT_TEXT_GLYPH_BITMAP_DESC_VERSION_1_SIZE ||
      glyph->font_key == 0 || !std::isfinite(glyph->bearing_x) ||
      !std::isfinite(glyph->bearing_y) || !zero(glyph->reserved, std::size(glyph->reserved)) ||
      ((glyph->width == 0) != (glyph->height == 0))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto empty = glyph->width == 0;
  const auto bytes_per_row = glyph->bytes_per_row == 0 ? glyph->width : glyph->bytes_per_row;
  if ((empty &&
       (glyph->bitmap != nullptr || glyph->bitmap_size != 0 || glyph->bytes_per_row != 0)) ||
      (!empty && (glyph->bitmap == nullptr || bytes_per_row < glyph->width ||
                  glyph->bitmap_size <
                      static_cast<uint64_t>(bytes_per_row) * (glyph->height - 1) + glyph->width))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::scoped_lock lock{state->mutex};
  if (glyph->width > state->page_width - state->padding * 2 ||
      glyph->height > state->page_height - state->padding * 2) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  const glyph_key key{glyph->font_key, glyph->glyph_id};
  const auto existing = state->glyphs.find(key);
  if (existing != state->glyphs.end()) {
    const auto& entry = existing->second;
    if (entry.width != glyph->width || entry.height != glyph->height ||
        entry.bearing_x != glyph->bearing_x || entry.bearing_y != glyph->bearing_y) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    return empty ? GRANIT_SUCCESS
                 : write_glyph(*state, state->pages[entry.page], *glyph, entry.x, entry.y);
  }
  try {
    if (empty) {
      state->glyphs.emplace(
          key, glyph_entry{UINT32_MAX, 0, 0, 0, 0, glyph->bearing_x, glyph->bearing_y});
      return GRANIT_SUCCESS;
    }
    uint32_t page_index = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t next_x = 0;
    uint32_t next_y = 0;
    uint32_t next_row_height = 0;
    while (page_index < state->pages.size() &&
           !find_position(*state, state->pages[page_index], glyph->width, glyph->height, x, y,
                          next_x, next_y, next_row_height)) {
      ++page_index;
    }
    if (page_index == state->pages.size()) {
      if (state->pages.size() >= state->max_pages)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      atlas_page page;
      auto result = create_page(*state, page);
      if (result != GRANIT_SUCCESS)
        return result;
      state->pages.push_back(std::move(page));
      if (!find_position(*state, state->pages.back(), glyph->width, glyph->height, x, y, next_x,
                         next_y, next_row_height)) {
        return GRANIT_ERROR_INTERNAL;
      }
    }
    auto result = write_glyph(*state, state->pages[page_index], *glyph, x, y);
    if (result != GRANIT_SUCCESS)
      return result;
    state->glyphs.emplace(key, glyph_entry{page_index, x, y, glyph->width, glyph->height,
                                           glyph->bearing_x, glyph->bearing_y});
    auto& page = state->pages[page_index];
    page.cursor_x = next_x;
    page.cursor_y = next_y;
    page.row_height = next_row_height;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_text_atlas_get_stats(granit_renderer renderer,
                                                     granit_text_atlas atlas,
                                                     granit_text_atlas_stats* stats) {
  if (stats == nullptr || stats->struct_size < GRANIT_TEXT_ATLAS_STATS_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto state = find(renderer, atlas);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  stats->glyph_count = static_cast<uint32_t>(state->glyphs.size());
  stats->page_count = static_cast<uint32_t>(state->pages.size());
  std::fill(std::begin(stats->reserved), std::end(stats->reserved), 0);
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_text_atlas_destroy(granit_renderer renderer,
                                                   granit_text_atlas atlas) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(atlas, index, generation))
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
