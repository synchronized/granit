// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/debug_draw_list.h>

#include <granit/renderer/buffer.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/shader.h>
#include <granit/renderer/texture.h>

#include "pipeline/debug_draw_geometry.h"
#include "pipeline/embedded_shaders.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x45);

struct list_state {
  ~list_state() {
    for (const auto pipeline : pipelines)
      static_cast<void>(granit_graphics_pipeline_destroy(renderer, pipeline.handle));
    if (pipeline_layout != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_pipeline_layout_destroy(renderer, pipeline_layout));
    if (srgb_fragment_shader != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_shader_destroy(renderer, srgb_fragment_shader));
    if (fragment_shader != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_shader_destroy(renderer, fragment_shader));
    if (vertex_shader != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_shader_destroy(renderer, vertex_shader));
    if (world_vertex_buffer != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_buffer_destroy(renderer, world_vertex_buffer));
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
  struct pipeline_entry {
    granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
    granit_texture_format depth_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
    bool depth_test = false;
    bool encode_srgb = false;
    granit_graphics_pipeline handle = GRANIT_NULL_HANDLE;
  };
  std::vector<pipeline_entry> pipelines;
  granit_pipeline_layout pipeline_layout = GRANIT_NULL_HANDLE;
  granit_shader vertex_shader = GRANIT_NULL_HANDLE;
  granit_shader fragment_shader = GRANIT_NULL_HANDLE;
  granit_shader srgb_fragment_shader = GRANIT_NULL_HANDLE;
  granit_buffer world_vertex_buffer = GRANIT_NULL_HANDLE;
  uint64_t world_vertex_capacity = 0;
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

bool valid_load_operation(granit_attachment_load_operation value) {
  return value == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD ||
         value == GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR ||
         value == GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD;
}

granit_result create_shader(granit_renderer renderer, granit_shader_stage stage,
                            std::span<const std::byte> code, granit_shader& shader) {
  granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  desc.stage = stage;
  desc.code = code.data();
  desc.code_size = code.size();
  desc.entry_point = stage == GRANIT_SHADER_STAGE_VERTEX ? "vertex_main" : "fragment_main";
  desc.entry_point_length = stage == GRANIT_SHADER_STAGE_VERTEX ? 11U : 13U;
  return granit_shader_create(renderer, &desc, &shader);
}

granit_result ensure_debug_shaders(list_state& state, bool encode_srgb) {
  auto result = GRANIT_SUCCESS;
  if (state.pipeline_layout == GRANIT_NULL_HANDLE) {
    const granit_pipeline_layout_desc desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
    result = granit_pipeline_layout_create(state.renderer, &desc, &state.pipeline_layout);
  }
  if (result == GRANIT_SUCCESS && state.vertex_shader == GRANIT_NULL_HANDLE) {
    result =
        create_shader(state.renderer, GRANIT_SHADER_STAGE_VERTEX,
                      granit::pipeline::detail::debug_world_vertex_shader(), state.vertex_shader);
  }
  auto& fragment = encode_srgb ? state.srgb_fragment_shader : state.fragment_shader;
  if (result == GRANIT_SUCCESS && fragment == GRANIT_NULL_HANDLE) {
    result =
        create_shader(state.renderer, GRANIT_SHADER_STAGE_FRAGMENT,
                      granit::pipeline::detail::debug_world_fragment_shader(encode_srgb), fragment);
  }
  return result;
}

granit_result acquire_debug_pipeline(list_state& state, granit_texture_format color_format,
                                     granit_texture_format depth_format, bool depth_test,
                                     bool encode_srgb, granit_graphics_pipeline& output) {
  const auto found = std::ranges::find_if(state.pipelines, [&](const auto& entry) {
    return entry.color_format == color_format && entry.depth_format == depth_format &&
           entry.depth_test == depth_test && entry.encode_srgb == encode_srgb;
  });
  if (found != state.pipelines.end()) {
    output = found->handle;
    return GRANIT_SUCCESS;
  }
  auto result = ensure_debug_shaders(state, encode_srgb);
  const std::array attributes{granit_vertex_attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X4, 0, 0},
                              granit_vertex_attribute{1, GRANIT_VERTEX_FORMAT_UINT32, 16, 0}};
  const granit_vertex_buffer_layout vertex_layout{
      sizeof(granit::pipeline::detail::debug_clip_vertex), GRANIT_VERTEX_STEP_MODE_VERTEX,
      static_cast<uint32_t>(attributes.size()), 0, attributes.data()};
  granit_depth_state depth = GRANIT_DEPTH_STATE_INIT;
  depth.test_enabled = depth_test ? 1U : 0U;
  depth.write_enabled = 0;
  granit_color_blend_state blend = GRANIT_COLOR_BLEND_STATE_INIT;
  blend.enabled = 1;
  blend.source_color_factor = GRANIT_BLEND_FACTOR_SOURCE_ALPHA;
  blend.destination_color_factor = GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA;
  blend.source_alpha_factor = GRANIT_BLEND_FACTOR_ONE;
  blend.destination_alpha_factor = GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA;
  granit_graphics_pipeline_desc desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  desc.layout = state.pipeline_layout;
  desc.vertex_shader = state.vertex_shader;
  desc.fragment_shader = encode_srgb ? state.srgb_fragment_shader : state.fragment_shader;
  desc.color_format_count = 1;
  desc.color_formats = &color_format;
  desc.depth_stencil_format = depth_format;
  desc.vertex_buffer_layout_count = 1;
  desc.vertex_buffer_layouts = &vertex_layout;
  desc.depth = &depth;
  desc.color_blend_count = 1;
  desc.color_blends = &blend;
  granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  if (result == GRANIT_SUCCESS)
    result = granit_graphics_pipeline_create(state.renderer, &desc, &pipeline);
  if (result != GRANIT_SUCCESS)
    return result;
  try {
    state.pipelines.push_back({color_format, depth_format, depth_test, encode_srgb, pipeline});
  } catch (...) {
    static_cast<void>(granit_graphics_pipeline_destroy(state.renderer, pipeline));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  output = pipeline;
  return GRANIT_SUCCESS;
}

granit_result
upload_world_vertices(list_state& state,
                      std::span<const granit::pipeline::detail::debug_clip_vertex> vertices) {
  const auto required = static_cast<uint64_t>(vertices.size_bytes());
  if (required == 0)
    return GRANIT_SUCCESS;
  if (state.world_vertex_buffer == GRANIT_NULL_HANDLE || state.world_vertex_capacity < required) {
    uint64_t capacity = 4096;
    while (capacity < required && capacity <= UINT64_MAX / 2)
      capacity *= 2;
    capacity = std::max(capacity, required);
    granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
    desc.size = capacity;
    desc.usage = GRANIT_BUFFER_USAGE_VERTEX_BIT;
    desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
    granit_buffer replacement = GRANIT_NULL_HANDLE;
    auto result = granit_buffer_create(state.renderer, &desc, &replacement);
    if (result != GRANIT_SUCCESS)
      return result;
    if (state.world_vertex_buffer != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_buffer_destroy(state.renderer, state.world_vertex_buffer));
    state.world_vertex_buffer = replacement;
    state.world_vertex_capacity = capacity;
  }
  return granit_buffer_write(state.renderer, state.world_vertex_buffer, 0, vertices.data(),
                             vertices.size_bytes());
}

} // namespace

extern "C" granit_result granit_debug_draw_list_create(granit_renderer renderer,
                                                       const granit_debug_draw_list_desc* desc,
                                                       granit_debug_draw_list* list) {
  if (list == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *list = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_DEBUG_DRAW_LIST_DESC_VERSION_1_SIZE ||
      !zero(desc->reserved, std::size(desc->reserved)))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
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
  if (stats == nullptr || stats->struct_size < GRANIT_DEBUG_DRAW_LIST_STATS_VERSION_1_SIZE)
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

extern "C" granit_result
granit_debug_draw_list_record_world(granit_renderer renderer, granit_command_recorder recorder,
                                    granit_debug_draw_list list,
                                    const granit_debug_draw_record_desc* desc) {
  if (recorder == GRANIT_NULL_HANDLE || desc == nullptr ||
      desc->struct_size < GRANIT_DEBUG_DRAW_RECORD_DESC_VERSION_1_SIZE ||
      !zero(desc->reserved, std::size(desc->reserved)) || desc->color == GRANIT_NULL_HANDLE ||
      desc->color_format == GRANIT_TEXTURE_FORMAT_UNDEFINED || desc->width == 0 ||
      desc->height == 0 || desc->encode_srgb > 1 ||
      !valid_load_operation(desc->color_load_operation) ||
      !valid_load_operation(desc->depth_load_operation) ||
      ((desc->depth == GRANIT_NULL_HANDLE) !=
       (desc->depth_format == GRANIT_TEXTURE_FORMAT_UNDEFINED)) ||
      !std::ranges::all_of(desc->view_projection,
                           [](float value) { return std::isfinite(value); })) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto state = find(renderer, list);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  struct draw_batch {
    uint32_t first_vertex = 0;
    uint32_t vertex_count = 0;
    bool depth_test = false;
  };
  try {
    std::scoped_lock lock{state->mutex};
    std::vector<granit::pipeline::detail::debug_clip_vertex> vertices;
    std::vector<draw_batch> batches;
    const auto append_vertices =
        [&](std::span<const granit::pipeline::detail::debug_clip_vertex> source, bool depth_test) {
          if (batches.empty() || batches.back().depth_test != depth_test) {
            batches.push_back({static_cast<uint32_t>(vertices.size()), 0, depth_test});
          }
          vertices.insert(vertices.end(), source.begin(), source.end());
          batches.back().vertex_count += static_cast<uint32_t>(source.size());
        };
    for (const auto& command : state->commands) {
      if (command.is_line && command.line.space == GRANIT_DEBUG_DRAW_SPACE_WORLD) {
        std::array<granit::pipeline::detail::debug_clip_vertex, 4> expanded{};
        const auto expanded_result = granit::pipeline::detail::expand_world_debug_line(
            command.line, desc->view_projection, desc->width, desc->height, expanded);
        if (expanded_result == granit::pipeline::detail::debug_line_expand_result::clipped)
          continue;
        if (expanded_result != granit::pipeline::detail::debug_line_expand_result::success)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        const std::array triangle_vertices{expanded[0], expanded[1], expanded[2],
                                           expanded[2], expanded[1], expanded[3]};
        append_vertices(triangle_vertices,
                        command.line.depth_mode == GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST);
      } else if (!command.is_line && command.triangle.space == GRANIT_DEBUG_DRAW_SPACE_WORLD) {
        std::array<granit::pipeline::detail::debug_clip_vertex, 3> triangle_vertices{};
        for (std::size_t index = 0; index < triangle_vertices.size(); ++index) {
          triangle_vertices[index] = granit::pipeline::detail::transform_world_debug_vertex(
              command.triangle.vertices[index], desc->view_projection);
        }
        append_vertices(triangle_vertices,
                        command.triangle.depth_mode == GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST);
      }
    }
    if (vertices.empty())
      return GRANIT_SUCCESS;
    if (std::ranges::any_of(batches, [](const auto& batch) { return batch.depth_test; }) &&
        desc->depth == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    auto result = upload_world_vertices(*state, vertices);
    std::vector<granit_graphics_pipeline> pipelines;
    pipelines.reserve(batches.size());
    for (const auto& batch : batches) {
      granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
      if (result == GRANIT_SUCCESS) {
        result = acquire_debug_pipeline(*state, desc->color_format, desc->depth_format,
                                        batch.depth_test, desc->encode_srgb != 0, pipeline);
      }
      if (result != GRANIT_SUCCESS)
        return result;
      pipelines.push_back(pipeline);
    }
    const granit_vertex_buffer_binding vertex_binding{state->world_vertex_buffer, 0};
    result = granit_command_recorder_bind_vertex_buffers(renderer, recorder, 0, &vertex_binding, 1);
    const granit_viewport viewport{
        0, 0, static_cast<float>(desc->width), static_cast<float>(desc->height), 0, 1};
    const granit_scissor scissor{0, 0, desc->width, desc->height};
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_viewports(renderer, recorder, 0, &viewport, 1);
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_set_scissors(renderer, recorder, 0, &scissor, 1);
    granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
    color.view = desc->color;
    color.load_operation = desc->color_load_operation;
    color.clear_value.alpha = 0;
    granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
    depth.view = desc->depth;
    depth.depth_load_operation = desc->depth_load_operation;
    granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
    rendering.color_attachment_count = 1;
    rendering.color_attachments = &color;
    rendering.depth_stencil_attachment = desc->depth == GRANIT_NULL_HANDLE ? nullptr : &depth;
    rendering.area = {0, 0, desc->width, desc->height};
    if (result == GRANIT_SUCCESS)
      result = granit_command_recorder_begin_rendering(renderer, recorder, &rendering);
    if (result == GRANIT_SUCCESS) {
      for (std::size_t index = 0; index < batches.size(); ++index) {
        result =
            granit_command_recorder_bind_graphics_pipeline(renderer, recorder, pipelines[index]);
        if (result == GRANIT_SUCCESS) {
          result = granit_command_recorder_draw(renderer, recorder, batches[index].vertex_count, 1,
                                                batches[index].first_vertex, 0);
        }
        if (result != GRANIT_SUCCESS)
          break;
      }
      const auto end_result = granit_command_recorder_end_rendering(renderer, recorder);
      if (result == GRANIT_SUCCESS)
        result = end_result;
    }
    return result;
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
