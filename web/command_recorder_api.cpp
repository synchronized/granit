// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/command_recorder.h>

#include "renderer/renderer_registry.h"

extern "C" granit_result granit_command_recorder_create(granit_renderer renderer,
                                                        const granit_command_recorder_desc* desc,
                                                        granit_command_recorder* recorder) {
  if (recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *recorder = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_COMMAND_RECORDER_DESC_VERSION_1_SIZE ||
      desc->flags != 0 || desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().create_command_recorder(renderer, *recorder);
}

extern "C" granit_result granit_command_recorder_begin(granit_renderer renderer,
                                                       granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().begin_command_recorder(renderer, recorder);
}

extern "C" granit_result granit_command_recorder_end(granit_renderer renderer,
                                                     granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().end_command_recorder(renderer, recorder);
}

extern "C" granit_result granit_command_recorder_submit(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().submit_command_recorder(renderer, recorder,
                                                                               GRANIT_NULL_HANDLE);
}

extern "C" granit_result
granit_command_recorder_submit_batch(granit_renderer, const granit_command_recorder*, uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_submit_frame(granit_renderer renderer,
                                                              granit_command_recorder recorder,
                                                              granit_frame frame) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      frame == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().submit_command_recorder(renderer, recorder,
                                                                               frame);
}

extern "C" granit_result granit_command_recorder_reset(granit_renderer renderer,
                                                       granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().reset_command_recorder(renderer, recorder);
}

extern "C" granit_result
granit_command_recorder_copy_buffer(granit_renderer, granit_command_recorder, granit_buffer,
                                    granit_buffer, const granit_buffer_copy_region*, uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_copy_texture_to_buffer(
    granit_renderer, granit_command_recorder, granit_texture, granit_buffer,
    const granit_texture_data_layout*, const granit_texture_write_region*) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_copy_buffer_to_texture(
    granit_renderer, granit_command_recorder, granit_buffer, granit_texture,
    const granit_texture_data_layout*, const granit_texture_write_region*) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_copy_texture(granit_renderer,
                                                              granit_command_recorder,
                                                              granit_texture, granit_texture,
                                                              const granit_texture_copy_region*) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result
granit_command_recorder_generate_mipmaps(granit_renderer, granit_command_recorder, granit_texture,
                                         const granit_texture_mipmap_range*) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_fill_buffer(granit_renderer,
                                                             granit_command_recorder, granit_buffer,
                                                             uint64_t, uint64_t, uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_bind_graphics_pipeline(
    granit_renderer renderer, granit_command_recorder recorder, granit_graphics_pipeline pipeline) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      pipeline == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().bind_graphics_pipeline(renderer, recorder,
                                                                              pipeline);
}

extern "C" granit_result
granit_command_recorder_bind_graphics_groups(granit_renderer, granit_command_recorder,
                                             granit_pipeline_layout,
                                             const granit_bind_groups_desc*) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_bind_compute_pipeline(granit_renderer,
                                                                       granit_command_recorder,
                                                                       granit_compute_pipeline) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result
granit_command_recorder_bind_compute_groups(granit_renderer, granit_command_recorder,
                                            granit_pipeline_layout,
                                            const granit_bind_groups_desc*) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_dispatch(granit_renderer, granit_command_recorder,
                                                          uint32_t, uint32_t, uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_set_viewports(granit_renderer,
                                                               granit_command_recorder, uint32_t,
                                                               const granit_viewport*, uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_set_scissors(granit_renderer,
                                                              granit_command_recorder, uint32_t,
                                                              const granit_scissor*, uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result
granit_command_recorder_bind_vertex_buffers(granit_renderer, granit_command_recorder, uint32_t,
                                            const granit_vertex_buffer_binding*, uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_command_recorder_bind_index_buffer(granit_renderer,
                                                                   granit_command_recorder,
                                                                   granit_buffer, uint64_t,
                                                                   granit_index_type) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result
granit_command_recorder_draw(granit_renderer renderer, granit_command_recorder recorder,
                             uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
                             uint32_t first_instance) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (vertex_count != 3 || instance_count != 1 || first_vertex != 0 || first_instance != 0)
    return GRANIT_ERROR_UNSUPPORTED;
  return granit::detail::renderer_registry::instance().draw(renderer, recorder);
}

extern "C" granit_result granit_command_recorder_draw_indexed(granit_renderer,
                                                              granit_command_recorder, uint32_t,
                                                              uint32_t, uint32_t, int32_t,
                                                              uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result
granit_command_recorder_begin_rendering(granit_renderer renderer, granit_command_recorder recorder,
                                        const granit_rendering_desc* desc) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_RENDERING_DESC_VERSION_1_SIZE ||
      desc->color_attachment_count != 1 || desc->color_attachments == nullptr ||
      desc->depth_stencil_attachment != nullptr || desc->layer_count != 1 || desc->reserved != 0 ||
      desc->reserved_2 != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& color = desc->color_attachments[0];
  if (color.struct_size < GRANIT_COLOR_ATTACHMENT_DESC_VERSION_1_SIZE || color.reserved != 0 ||
      color.reserved_2 != 0 || color.view == GRANIT_NULL_HANDLE ||
      color.load_operation != GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR ||
      color.store_operation != GRANIT_ATTACHMENT_STORE_OPERATION_STORE)
    return GRANIT_ERROR_UNSUPPORTED;
  return granit::detail::renderer_registry::instance().begin_rendering(renderer, recorder,
                                                                       color.view);
}

extern "C" granit_result granit_command_recorder_end_rendering(granit_renderer renderer,
                                                               granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().end_rendering(renderer, recorder);
}

extern "C" granit_result granit_command_recorder_destroy(granit_renderer renderer,
                                                         granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().destroy_command_recorder(renderer, recorder);
}
