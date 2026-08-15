// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/command_recorder.h>

#include <cmath>
#include <cstdint>
#include <new>

#include "core/resource_validation.h"
#include "renderer/renderer_registry.h"

extern "C" granit_result granit_command_recorder_create(granit_renderer renderer,
                                                        const granit_command_recorder_desc* desc,
                                                        granit_command_recorder* recorder) {
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr || recorder == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *recorder = GRANIT_NULL_HANDLE;
  if (desc->struct_size < GRANIT_COMMAND_RECORDER_DESC_VERSION_1_SIZE || desc->flags != 0 ||
      desc->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return granit::detail::renderer_registry::instance().create_command_recorder(renderer, *recorder);
}

extern "C" granit_result granit_command_recorder_begin(granit_renderer renderer,
                                                       granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().begin_command_recorder(renderer, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_end(granit_renderer renderer,
                                                     granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().end_command_recorder(renderer, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_submit(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().submit_command_recorder(renderer,
                                                                                 recorder);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_submit_batch(
    granit_renderer renderer, const granit_command_recorder* recorders, uint32_t recorder_count) {
  if (renderer == GRANIT_NULL_HANDLE || recorders == nullptr || recorder_count == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return granit::detail::renderer_registry::instance().submit_command_recorders(
        renderer, std::span{recorders, recorder_count});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_submit_frame(granit_renderer renderer,
                                                              granit_command_recorder recorder,
                                                              granit_frame frame) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      frame == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().submit_command_recorder_frame(
        renderer, recorder, frame);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_reset(granit_renderer renderer,
                                                       granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().reset_command_recorder(renderer, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_command_recorder_copy_buffer(granit_renderer renderer, granit_command_recorder recorder,
                                    granit_buffer source, granit_buffer destination,
                                    const granit_buffer_copy_region* regions,
                                    std::uint32_t region_count) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      source == GRANIT_NULL_HANDLE || destination == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (regions == nullptr || region_count == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return granit::detail::renderer_registry::instance().copy_buffer(
        renderer, recorder, source, destination, {regions, region_count});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_copy_texture_to_buffer(
    granit_renderer renderer, granit_command_recorder recorder, granit_texture source,
    granit_buffer destination, const granit_texture_data_layout* layout,
    const granit_texture_write_region* region) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      source == GRANIT_NULL_HANDLE || destination == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (layout == nullptr || region == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().copy_texture_to_buffer(
        renderer, recorder, source, destination, *layout, *region);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_command_recorder_copy_texture(granit_renderer renderer, granit_command_recorder recorder,
                                     granit_texture source, granit_texture destination,
                                     const granit_texture_copy_region* region) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      source == GRANIT_NULL_HANDLE || destination == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (region == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().copy_texture(renderer, recorder, source,
                                                                      destination, *region);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_command_recorder_fill_buffer(granit_renderer renderer, granit_command_recorder recorder,
                                    granit_buffer buffer, std::uint64_t offset, std::uint64_t size,
                                    std::uint32_t value) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().fill_buffer(renderer, recorder, buffer,
                                                                     offset, size, value);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_bind_graphics_pipeline(
    granit_renderer renderer, granit_command_recorder recorder, granit_graphics_pipeline pipeline) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      pipeline == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().bind_graphics_pipeline(renderer, recorder,
                                                                                pipeline);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_bind_graphics_groups(
    granit_renderer renderer, granit_command_recorder recorder, granit_pipeline_layout layout,
    uint32_t first_group, const granit_bind_group* bind_groups, uint32_t bind_group_count) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      layout == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!bind_groups || bind_group_count == 0 || first_group > 8 || bind_group_count > 8 ||
      first_group + bind_group_count > 8)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().bind_graphics_groups(
        renderer, recorder, layout, first_group, {bind_groups, bind_group_count});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_bind_compute_pipeline(
    granit_renderer renderer, granit_command_recorder recorder, granit_compute_pipeline pipeline) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      pipeline == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().bind_compute_pipeline(renderer, recorder,
                                                                               pipeline);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_bind_compute_groups(
    granit_renderer renderer, granit_command_recorder recorder, granit_pipeline_layout layout,
    uint32_t first_group, const granit_bind_group* bind_groups, uint32_t bind_group_count) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      layout == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!bind_groups || bind_group_count == 0 || first_group > 8 || bind_group_count > 8 ||
      first_group + bind_group_count > 8)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().bind_compute_groups(
        renderer, recorder, layout, first_group, {bind_groups, bind_group_count});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_dispatch(granit_renderer renderer,
                                                          granit_command_recorder recorder,
                                                          uint32_t group_count_x,
                                                          uint32_t group_count_y,
                                                          uint32_t group_count_z) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (group_count_x == 0 || group_count_y == 0 || group_count_z == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().dispatch(renderer, recorder, group_count_x,
                                                                  group_count_y, group_count_z);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_set_viewports(granit_renderer renderer,
                                                               granit_command_recorder recorder,
                                                               uint32_t first_viewport,
                                                               const granit_viewport* viewports,
                                                               uint32_t viewport_count) {
  if (!viewports || viewport_count == 0 || first_viewport + viewport_count > 16)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint32_t index = 0; index < viewport_count; ++index) {
    const auto& value = viewports[index];
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.width) ||
        !std::isfinite(value.height) || !std::isfinite(value.min_depth) ||
        !std::isfinite(value.max_depth) || value.width <= 0 || value.height <= 0 ||
        value.min_depth < 0 || value.max_depth > 1 || value.min_depth > value.max_depth)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return granit::detail::renderer_registry::instance().set_viewports(
        renderer, recorder, first_viewport, {viewports, viewport_count});
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_set_scissors(granit_renderer renderer,
                                                              granit_command_recorder recorder,
                                                              uint32_t first_scissor,
                                                              const granit_scissor* scissors,
                                                              uint32_t scissor_count) {
  if (!scissors || scissor_count == 0 || first_scissor + scissor_count > 16)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint32_t index = 0; index < scissor_count; ++index) {
    if (scissors[index].width == 0 || scissors[index].height == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return granit::detail::renderer_registry::instance().set_scissors(
        renderer, recorder, first_scissor, {scissors, scissor_count});
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_bind_vertex_buffers(
    granit_renderer renderer, granit_command_recorder recorder, uint32_t first_binding,
    const granit_vertex_buffer_binding* bindings, uint32_t binding_count) {
  if (!bindings || binding_count == 0 || first_binding + binding_count > 16)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().bind_vertex_buffers(
        renderer, recorder, first_binding, {bindings, binding_count});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_bind_index_buffer(granit_renderer renderer,
                                                                   granit_command_recorder recorder,
                                                                   granit_buffer buffer,
                                                                   uint64_t offset,
                                                                   granit_index_type index_type) {
  if (buffer == GRANIT_NULL_HANDLE ||
      (index_type != GRANIT_INDEX_TYPE_UINT16 && index_type != GRANIT_INDEX_TYPE_UINT32))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().bind_index_buffer(
        renderer, recorder, buffer, offset, index_type);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_command_recorder_draw(granit_renderer renderer, granit_command_recorder recorder,
                             uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
                             uint32_t first_instance) {
  try {
    return granit::detail::renderer_registry::instance().draw(
        renderer, recorder, vertex_count, instance_count, first_vertex, first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_draw_indexed(
    granit_renderer renderer, granit_command_recorder recorder, uint32_t index_count,
    uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
  try {
    return granit::detail::renderer_registry::instance().draw_indexed(
        renderer, recorder, index_count, instance_count, first_index, vertex_offset,
        first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_command_recorder_begin_rendering(granit_renderer renderer, granit_command_recorder recorder,
                                        const granit_rendering_desc* desc) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto validation = granit::detail::validate_rendering_desc(*desc);
  if (validation != GRANIT_SUCCESS)
    return validation;
  try {
    return granit::detail::renderer_registry::instance().begin_rendering(renderer, recorder, *desc);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_end_rendering(granit_renderer renderer,
                                                               granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().end_rendering(renderer, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_destroy(granit_renderer renderer,
                                                         granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().destroy_command_recorder(renderer,
                                                                                  recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
