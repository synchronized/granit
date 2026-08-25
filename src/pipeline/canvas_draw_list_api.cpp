// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/canvas_draw_list.h>

#include "pipeline/canvas_draw_list.h"
#include "pipeline/canvas_geometry_upload.h"
#include "pipeline/canvas_pass.h"
#include "pipeline/embedded_shaders.h"

#include <granit/pipeline/material.h>

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

struct canvas_draw_list_state {
  explicit canvas_draw_list_state(uint32_t frame_slot_count)
      : geometry(frame_slot_count), bindings(frame_slot_count) {}

  ~canvas_draw_list_state() {
    for (auto& binding : bindings)
      static_cast<void>(binding.reset());
    if (material != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_material_destroy(renderer, material));
  }

  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit::pipeline::detail::canvas_draw_list list;
  granit::pipeline::detail::canvas_geometry_upload geometry;
  std::vector<granit::pipeline::detail::pbr_draw_bindings> bindings;
  granit_material material = GRANIT_NULL_HANDLE;
};

struct canvas_draw_list_slot {
  std::shared_ptr<canvas_draw_list_state> state;
  uint32_t generation = 1;
};

std::mutex registry_mutex;
std::vector<canvas_draw_list_slot> registry;

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

std::shared_ptr<canvas_draw_list_state> find_list(granit_renderer renderer,
                                                  granit_canvas_draw_list list) {
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

bool valid_state(const granit_canvas_draw_state& state) {
  return state.texture != GRANIT_NULL_HANDLE && state.sampler != GRANIT_NULL_HANDLE;
}

granit::pipeline::detail::canvas_draw_state convert_state(const granit_canvas_draw_state& state) {
  return {.texture = state.texture, .sampler = state.sampler, .scissor = state.scissor};
}

granit_result ensure_material(canvas_draw_list_state& state) {
  if (state.material != GRANIT_NULL_HANDLE)
    return GRANIT_SUCCESS;
  const auto items = state.list.items();
  if (items.empty())
    return GRANIT_SUCCESS;
  const std::array<float, 4> white{1, 1, 1, 1};
  const std::array updates{
      granit_material_parameter_update{granit_material_parameter_id("base_color", 10),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, white.data(),
                                       sizeof(white), GRANIT_NULL_HANDLE},
      granit_material_parameter_update{granit_material_parameter_id("base_color_texture", 18),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       items.front().state.texture},
      granit_material_parameter_update{granit_material_parameter_id("unlit_sampler", 13),
                                       GRANIT_MATERIAL_PARAMETER_SAMPLER, 0, nullptr, 0,
                                       items.front().state.sampler}};
  const auto archive = granit::pipeline::detail::canvas_material_package();
  granit_material_desc desc = GRANIT_MATERIAL_DESC_INIT;
  desc.archive_data = archive.data();
  desc.archive_size = archive.size();
  desc.initial_updates = updates.data();
  desc.initial_update_count = static_cast<uint32_t>(updates.size());
  return granit_material_create(state.renderer, &desc, &state.material);
}

granit::material::pbr_matrix4 pixel_projection(uint32_t width, uint32_t height) {
  return {2.0F / static_cast<float>(width),
          0,
          0,
          0,
          0,
          2.0F / static_cast<float>(height),
          0,
          0,
          0,
          0,
          1,
          0,
          -1,
          -1,
          0,
          1};
}

constexpr granit::material::pbr_matrix4 identity_matrix() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

} // namespace

extern "C" granit_result granit_canvas_draw_list_create(granit_renderer renderer,
                                                        const granit_canvas_draw_list_desc* desc,
                                                        granit_canvas_draw_list* list) {
  if (list == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *list = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr ||
      desc->struct_size < GRANIT_CANVAS_DRAW_LIST_DESC_VERSION_1_SIZE ||
      !reserved_is_zero(desc->reserved, std::size(desc->reserved))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  uint64_t pipeline_cache_size = 0;
  const auto renderer_result =
      granit_renderer_pipeline_cache_export(renderer, nullptr, &pipeline_cache_size);
  if (renderer_result != GRANIT_SUCCESS)
    return renderer_result;
  try {
    const auto frame_slot_count = desc->frame_slot_count;
    if (frame_slot_count == 0 || frame_slot_count > GRANIT_MAX_FRAMES_IN_FLIGHT) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    auto state = std::make_shared<canvas_draw_list_state>(frame_slot_count);
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

extern "C" granit_result granit_canvas_draw_list_clear(granit_renderer renderer,
                                                       granit_canvas_draw_list list) {
  const auto state = find_list(renderer, list);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  state->list.clear();
  return GRANIT_SUCCESS;
}

extern "C" granit_result
granit_canvas_draw_list_append(granit_renderer renderer, granit_canvas_draw_list list,
                               const granit_canvas_vertex* vertices, uint32_t vertex_count,
                               const uint32_t* indices, uint32_t index_count,
                               const granit_canvas_draw_state* draw_state) {
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

extern "C" granit_result
granit_canvas_draw_list_append_batch(granit_renderer renderer, granit_canvas_draw_list list,
                                     const granit_canvas_vertex* vertices, uint32_t vertex_count,
                                     const uint32_t* indices, uint32_t index_count,
                                     const granit_canvas_draw_range* ranges, uint32_t range_count) {
  const auto state = find_list(renderer, list);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (vertices == nullptr || vertex_count == 0 || indices == nullptr || index_count == 0 ||
      index_count % 3 != 0 || ranges == nullptr || range_count == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (uint32_t index = 0; index < vertex_count; ++index) {
    const auto& vertex = vertices[index];
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.u) ||
        !std::isfinite(vertex.v)) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  for (uint32_t index = 0; index < range_count; ++index) {
    if (!valid_state(ranges[index].state) || ranges[index].index_count % 3 != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::scoped_lock lock{state->mutex};
  return state->list.append_batch(std::span{vertices, vertex_count},
                                  std::span{indices, index_count}, std::span{ranges, range_count});
}

extern "C" granit_result granit_canvas_draw_list_append_rect(granit_renderer renderer,
                                                             granit_canvas_draw_list list,
                                                             const granit_canvas_rect_desc* desc) {
  if (desc == nullptr || desc->struct_size < GRANIT_CANVAS_RECT_DESC_VERSION_1_SIZE ||
      !reserved_is_zero(desc->reserved, std::size(desc->reserved)) || !std::isfinite(desc->x) ||
      !std::isfinite(desc->y) || !std::isfinite(desc->width) || !std::isfinite(desc->height) ||
      !std::isfinite(desc->u0) || !std::isfinite(desc->v0) || !std::isfinite(desc->u1) ||
      !std::isfinite(desc->v1) || desc->width <= 0.0F || desc->height <= 0.0F) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::array vertices{
      granit_canvas_vertex{desc->x, desc->y, desc->u0, desc->v0, desc->color},
      granit_canvas_vertex{desc->x + desc->width, desc->y, desc->u1, desc->v0, desc->color},
      granit_canvas_vertex{desc->x + desc->width, desc->y + desc->height, desc->u1, desc->v1,
                           desc->color},
      granit_canvas_vertex{desc->x, desc->y + desc->height, desc->u0, desc->v1, desc->color}};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 2, 3, 0};
  return granit_canvas_draw_list_append(renderer, list, vertices.data(),
                                        static_cast<uint32_t>(vertices.size()), indices.data(),
                                        static_cast<uint32_t>(indices.size()), &desc->state);
}

extern "C" granit_result granit_canvas_draw_list_get_stats(granit_renderer renderer,
                                                           granit_canvas_draw_list list,
                                                           granit_canvas_draw_list_stats* stats) {
  if (stats == nullptr || stats->struct_size < GRANIT_CANVAS_DRAW_LIST_STATS_VERSION_1_SIZE)
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

extern "C" granit_result granit_canvas_draw_list_record(granit_renderer renderer,
                                                        granit_command_recorder recorder,
                                                        granit_canvas_draw_list list,
                                                        const granit_canvas_record_desc* desc) {
  if (recorder == GRANIT_NULL_HANDLE || desc == nullptr ||
      desc->struct_size < GRANIT_CANVAS_RECORD_DESC_VERSION_1_SIZE ||
      !reserved_is_zero(desc->reserved, std::size(desc->reserved)) ||
      desc->color == GRANIT_NULL_HANDLE || desc->color_format == GRANIT_TEXTURE_FORMAT_UNDEFINED ||
      desc->width == 0 || desc->height == 0 || desc->encode_srgb > 1 ||
      (desc->load_operation != GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD &&
       desc->load_operation != GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR &&
       desc->load_operation != GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto frame_slot = desc->frame_slot;
  const auto state = find_list(renderer, list);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{state->mutex};
  if (frame_slot != GRANIT_CANVAS_FRAME_SLOT_AUTO &&
      frame_slot >= state->geometry.frame_slot_count()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (state->list.items().empty())
    return GRANIT_SUCCESS;
  auto result = ensure_material(*state);
  if (result == GRANIT_SUCCESS)
    result = state->geometry.upload(renderer, state->list, frame_slot);
  const granit::material::pbr_frame_constants frame{.view_projection =
                                                        pixel_projection(desc->width, desc->height),
                                                    .camera_position = {},
                                                    .direction_to_light = {},
                                                    .light_radiance = {}};
  const granit::material::pbr_object_constants object{
      .model = identity_matrix(), .normal_matrix = identity_matrix(), .object_id = {}};
  if (result == GRANIT_SUCCESS) {
    result = granit::pipeline::detail::record_canvas_pass(
        renderer, recorder,
        {.color = desc->color,
         .color_format = desc->color_format,
         .width = desc->width,
         .height = desc->height,
         .material = state->material,
         .frame = frame,
         .object = object,
         .load_operation = desc->load_operation,
         .encode_srgb = desc->encode_srgb != 0},
        state->list, state->geometry, state->bindings[state->geometry.current_frame_slot()]);
  }
  return result;
}

extern "C" granit_result granit_canvas_draw_list_destroy(granit_renderer renderer,
                                                         granit_canvas_draw_list list) {
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
