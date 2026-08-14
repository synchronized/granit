// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/mesh.h>

#include "pipeline/mesh_access.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x43);

struct mesh_vertex_buffer {
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  uint64_t offset = 0;
  uint32_t stride = 0;
  granit_vertex_step_mode step_mode = GRANIT_VERTEX_STEP_MODE_VERTEX;
  std::vector<granit_vertex_attribute> attributes;
};

struct mesh_state {
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_primitive_topology topology = GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  std::vector<mesh_vertex_buffer> vertex_buffers;
  granit_buffer index_buffer = GRANIT_NULL_HANDLE;
  uint64_t index_buffer_offset = 0;
  granit_index_type index_type = GRANIT_INDEX_TYPE_UINT16;
  uint32_t vertex_count = 0;
  uint32_t index_count = 0;
  uint32_t instance_count = 1;
  uint32_t first_vertex = 0;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
};

struct mesh_slot {
  std::shared_ptr<mesh_state> state;
  uint32_t generation = 1;
};

std::mutex registry_mutex;
std::vector<mesh_slot> registry;

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

uint32_t format_size(granit_vertex_format format) {
  switch (format) {
  case GRANIT_VERTEX_FORMAT_FLOAT32:
  case GRANIT_VERTEX_FORMAT_UINT32:
  case GRANIT_VERTEX_FORMAT_SINT32:
    return 4;
  case GRANIT_VERTEX_FORMAT_FLOAT32X2:
  case GRANIT_VERTEX_FORMAT_UINT32X2:
  case GRANIT_VERTEX_FORMAT_SINT32X2:
    return 8;
  case GRANIT_VERTEX_FORMAT_FLOAT32X3:
  case GRANIT_VERTEX_FORMAT_UINT32X3:
  case GRANIT_VERTEX_FORMAT_SINT32X3:
    return 12;
  case GRANIT_VERTEX_FORMAT_FLOAT32X4:
  case GRANIT_VERTEX_FORMAT_UINT32X4:
  case GRANIT_VERTEX_FORMAT_SINT32X4:
    return 16;
  default:
    return 0;
  }
}

bool valid_topology(granit_primitive_topology value) {
  return value >= GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST &&
         value <= GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
}

granit_result validate_buffer(granit_renderer renderer, granit_buffer buffer, uint64_t offset,
                              granit_buffer_usage usage, uint64_t required) {
  granit_buffer_desc desc{};
  const auto result = granit_buffer_get_desc(renderer, buffer, &desc);
  if (result != GRANIT_SUCCESS)
    return result;
  if ((desc.usage & usage) == 0 || offset > desc.size || required > desc.size - offset)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return GRANIT_SUCCESS;
}

} // namespace

granit_result granit::pipeline::detail::validate_mesh_handle(granit_renderer renderer,
                                                             granit_mesh mesh) noexcept {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(mesh, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::scoped_lock lock{registry_mutex};
  return index < registry.size() && registry[index].generation == generation &&
                 registry[index].state != nullptr && registry[index].state->renderer == renderer
             ? GRANIT_SUCCESS
             : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result
granit::pipeline::detail::copy_mesh_pipeline_state(granit_renderer renderer, granit_mesh mesh,
                                                   mesh_pipeline_state& output) noexcept {
  output = {};
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(mesh, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<mesh_state> state;
  {
    std::scoped_lock lock{registry_mutex};
    if (index >= registry.size() || registry[index].generation != generation ||
        registry[index].state == nullptr || registry[index].state->renderer != renderer) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    state = registry[index].state;
  }
  try {
    mesh_pipeline_state replacement{.topology = state->topology, .vertex_buffers = {}};
    replacement.vertex_buffers.reserve(state->vertex_buffers.size());
    bool has_position = false;
    for (const auto& source : state->vertex_buffers) {
      replacement.vertex_buffers.push_back({.stride = source.stride,
                                            .step_mode = source.step_mode,
                                            .attributes = source.attributes});
      for (const auto& attribute : source.attributes) {
        if (attribute.location == 0 && attribute.format == GRANIT_VERTEX_FORMAT_FLOAT32X3 &&
            source.step_mode == GRANIT_VERTEX_STEP_MODE_VERTEX) {
          has_position = true;
        }
      }
    }
    if (!has_position)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    output = std::move(replacement);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result granit::pipeline::detail::bind_mesh_buffers(granit_renderer renderer,
                                                          granit_command_recorder recorder,
                                                          granit_mesh mesh) noexcept {
  size_t index = 0;
  uint32_t generation = 0;
  if (recorder == GRANIT_NULL_HANDLE || !decode(mesh, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<mesh_state> state;
  {
    std::scoped_lock lock{registry_mutex};
    if (index >= registry.size() || registry[index].generation != generation ||
        registry[index].state == nullptr || registry[index].state->renderer != renderer) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    state = registry[index].state;
  }
  try {
    std::vector<granit_vertex_buffer_binding> bindings;
    bindings.reserve(state->vertex_buffers.size());
    for (const auto& vertex : state->vertex_buffers) {
      granit_buffer_desc desc{};
      const auto result = granit_buffer_get_desc(renderer, vertex.buffer, &desc);
      if (result != GRANIT_SUCCESS)
        return result;
      if ((desc.usage & GRANIT_BUFFER_USAGE_VERTEX_BIT) == 0 || vertex.offset >= desc.size)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      bindings.push_back({vertex.buffer, vertex.offset});
    }
    auto result = granit_command_recorder_bind_vertex_buffers(
        renderer, recorder, 0, bindings.data(), static_cast<uint32_t>(bindings.size()));
    if (result != GRANIT_SUCCESS)
      return result;
    if (state->index_buffer != GRANIT_NULL_HANDLE) {
      granit_buffer_desc desc{};
      result = granit_buffer_get_desc(renderer, state->index_buffer, &desc);
      if (result != GRANIT_SUCCESS)
        return result;
      if ((desc.usage & GRANIT_BUFFER_USAGE_INDEX_BIT) == 0 ||
          state->index_buffer_offset >= desc.size) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      result = granit_command_recorder_bind_index_buffer(
          renderer, recorder, state->index_buffer, state->index_buffer_offset, state->index_type);
      return result;
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result granit::pipeline::detail::draw_mesh(granit_renderer renderer,
                                                  granit_command_recorder recorder,
                                                  granit_mesh mesh) noexcept {
  size_t index = 0;
  uint32_t generation = 0;
  if (recorder == GRANIT_NULL_HANDLE || !decode(mesh, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<mesh_state> state;
  {
    std::scoped_lock lock{registry_mutex};
    if (index >= registry.size() || registry[index].generation != generation ||
        registry[index].state == nullptr || registry[index].state->renderer != renderer) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    state = registry[index].state;
  }
  if (state->index_buffer != GRANIT_NULL_HANDLE) {
    return granit_command_recorder_draw_indexed(renderer, recorder, state->index_count,
                                                state->instance_count, state->first_index,
                                                state->vertex_offset, state->first_instance);
  }
  return granit_command_recorder_draw(renderer, recorder, state->vertex_count,
                                      state->instance_count, state->first_vertex,
                                      state->first_instance);
}

extern "C" granit_result granit_mesh_create(granit_renderer renderer, const granit_mesh_desc* desc,
                                            granit_mesh* mesh) {
  if (mesh == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *mesh = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr ||
      desc->struct_size < sizeof(granit_mesh_desc) || desc->reserved != 0 ||
      !valid_topology(desc->topology) || desc->vertex_buffer_count == 0 ||
      desc->vertex_buffer_count > 16 || desc->vertex_buffers == nullptr ||
      desc->instance_count == 0 || desc->indexed > 1 ||
      (desc->indexed == 0 && (desc->vertex_count == 0 || desc->index_buffer != GRANIT_NULL_HANDLE ||
                              desc->index_count != 0)) ||
      (desc->indexed != 0 && (desc->index_count == 0 || desc->index_buffer == GRANIT_NULL_HANDLE ||
                              (desc->index_type != GRANIT_INDEX_TYPE_UINT16 &&
                               desc->index_type != GRANIT_INDEX_TYPE_UINT32)))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    auto state = std::make_shared<mesh_state>();
    state->renderer = renderer;
    state->topology = desc->topology;
    state->vertex_buffers.reserve(desc->vertex_buffer_count);
    std::unordered_set<uint32_t> locations;
    for (uint32_t slot = 0; slot < desc->vertex_buffer_count; ++slot) {
      const auto& source = desc->vertex_buffers[slot];
      if (source.buffer == GRANIT_NULL_HANDLE || source.layout.stride == 0 ||
          source.layout.attribute_count == 0 || source.layout.attributes == nullptr ||
          source.layout.reserved != 0 ||
          (source.layout.step_mode != GRANIT_VERTEX_STEP_MODE_VERTEX &&
           source.layout.step_mode != GRANIT_VERTEX_STEP_MODE_INSTANCE)) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      mesh_vertex_buffer target{.buffer = source.buffer,
                                .offset = source.offset,
                                .stride = source.layout.stride,
                                .step_mode = source.layout.step_mode,
                                .attributes = {}};
      target.attributes.assign(source.layout.attributes,
                               source.layout.attributes + source.layout.attribute_count);
      for (const auto& attribute : target.attributes) {
        const auto size = format_size(attribute.format);
        if (attribute.reserved != 0 || size == 0 || attribute.offset > target.stride ||
            size > target.stride - attribute.offset || !locations.insert(attribute.location).second)
          return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      const auto elements = source.layout.step_mode == GRANIT_VERTEX_STEP_MODE_INSTANCE
                                ? static_cast<uint64_t>(desc->first_instance) + desc->instance_count
                            : desc->indexed != 0
                                ? UINT64_C(1)
                                : static_cast<uint64_t>(desc->first_vertex) + desc->vertex_count;
      const auto required = static_cast<uint64_t>(target.stride) * elements;
      const auto result = validate_buffer(renderer, source.buffer, source.offset,
                                          GRANIT_BUFFER_USAGE_VERTEX_BIT, required);
      if (result != GRANIT_SUCCESS)
        return result;
      state->vertex_buffers.push_back(std::move(target));
    }
    if (desc->indexed != 0) {
      const auto index_size = desc->index_type == GRANIT_INDEX_TYPE_UINT16 ? 2U : 4U;
      if (desc->index_buffer_offset % index_size != 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto required =
          (static_cast<uint64_t>(desc->first_index) + desc->index_count) * index_size;
      const auto result = validate_buffer(renderer, desc->index_buffer, desc->index_buffer_offset,
                                          GRANIT_BUFFER_USAGE_INDEX_BIT, required);
      if (result != GRANIT_SUCCESS)
        return result;
      state->index_buffer = desc->index_buffer;
      state->index_buffer_offset = desc->index_buffer_offset;
      state->index_type = desc->index_type;
    }
    state->vertex_count = desc->vertex_count;
    state->index_count = desc->index_count;
    state->instance_count = desc->instance_count;
    state->first_vertex = desc->first_vertex;
    state->first_index = desc->first_index;
    state->vertex_offset = desc->vertex_offset;
    state->first_instance = desc->first_instance;
    std::scoped_lock lock{registry_mutex};
    size_t index = 0;
    while (index < registry.size() && registry[index].state != nullptr)
      ++index;
    if (index == registry.size())
      registry.emplace_back();
    registry[index].state = std::move(state);
    *mesh = encode(index, registry[index].generation);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_mesh_destroy(granit_renderer renderer, granit_mesh mesh) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(mesh, index, generation))
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
