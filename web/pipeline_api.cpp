// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/pipeline.h>

#include "renderer/renderer_registry.h"

extern "C" granit_result granit_bind_group_layout_create(granit_renderer,
                                                         const granit_bind_group_layout_desc*,
                                                         granit_bind_group_layout* layout) {
  if (layout != nullptr) {
    *layout = GRANIT_NULL_HANDLE;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_bind_group_layout_destroy(granit_renderer,
                                                          granit_bind_group_layout) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_bind_group_create(granit_renderer, const granit_bind_group_desc*,
                                                  granit_bind_group* bind_group) {
  if (bind_group != nullptr) {
    *bind_group = GRANIT_NULL_HANDLE;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_bind_group_destroy(granit_renderer, granit_bind_group) {
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_pipeline_layout_create(granit_renderer renderer,
                                                       const granit_pipeline_layout_desc* desc,
                                                       granit_pipeline_layout* layout) {
  if (layout == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *layout = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (desc == nullptr || desc->struct_size < GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 ||
      (desc->bind_group_layout_count != 0 && desc->bind_group_layouts == nullptr)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc->bind_group_layout_count != 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return granit::detail::renderer_registry::instance().create_pipeline_layout(renderer, *layout);
}

extern "C" granit_result granit_pipeline_layout_destroy(granit_renderer renderer,
                                                        granit_pipeline_layout layout) {
  if (renderer == GRANIT_NULL_HANDLE || layout == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::renderer_registry::instance().destroy_pipeline_layout(renderer, layout);
}

extern "C" granit_result granit_graphics_pipeline_create(granit_renderer renderer,
                                                         const granit_graphics_pipeline_desc* desc,
                                                         granit_graphics_pipeline* pipeline) {
  if (pipeline == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *pipeline = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (desc == nullptr || desc->struct_size < GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->reserved_2 != 0 || desc->layout == GRANIT_NULL_HANDLE ||
      desc->vertex_shader == GRANIT_NULL_HANDLE || desc->fragment_shader == GRANIT_NULL_HANDLE ||
      desc->color_format_count != 1 || desc->color_formats == nullptr ||
      (desc->color_formats[0] != GRANIT_TEXTURE_FORMAT_RGBA8_UNORM &&
       desc->color_formats[0] != GRANIT_TEXTURE_FORMAT_BGRA8_UNORM) ||
      desc->depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED ||
      desc->sample_count != GRANIT_SAMPLE_COUNT_1) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_2_SIZE &&
      (desc->reserved_3 != 0 || desc->vertex_buffer_layout_count != 0)) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_3_SIZE &&
      (desc->primitive.topology != GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
       desc->primitive.front_face != GRANIT_FRONT_FACE_COUNTER_CLOCKWISE ||
       desc->primitive.cull_mode != GRANIT_CULL_MODE_NONE ||
       desc->primitive.polygon_mode != GRANIT_POLYGON_MODE_FILL)) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_4_SIZE &&
      (desc->reserved_4 != 0 || desc->depth != nullptr || desc->color_blend_count != 0)) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE &&
      desc->depth_bias != nullptr) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return granit::detail::renderer_registry::instance().create_graphics_pipeline(
      renderer, desc->layout, desc->vertex_shader, desc->fragment_shader, desc->color_formats[0],
      *pipeline);
}

extern "C" granit_result granit_graphics_pipeline_destroy(granit_renderer renderer,
                                                          granit_graphics_pipeline pipeline) {
  if (renderer == GRANIT_NULL_HANDLE || pipeline == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::renderer_registry::instance().destroy_graphics_pipeline(renderer,
                                                                                 pipeline);
}

extern "C" granit_result granit_compute_pipeline_create(granit_renderer,
                                                        const granit_compute_pipeline_desc*,
                                                        granit_compute_pipeline* pipeline) {
  if (pipeline != nullptr) {
    *pipeline = GRANIT_NULL_HANDLE;
  }
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_compute_pipeline_destroy(granit_renderer, granit_compute_pipeline) {
  return GRANIT_ERROR_UNSUPPORTED;
}
