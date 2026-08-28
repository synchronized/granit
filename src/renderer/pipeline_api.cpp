// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/pipeline.h>

#include "renderer/renderer_registry.h"

#include <array>
#include <cmath>

namespace {

bool valid_format(granit_texture_format format) noexcept {
  return format >= GRANIT_TEXTURE_FORMAT_R8_UNORM &&
         format <= GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
}

uint32_t vertex_format_size(granit_vertex_format format) noexcept {
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

} // namespace

extern "C" granit_result granit_bind_group_layout_create(granit_renderer renderer,
                                                         const granit_bind_group_layout_desc* desc,
                                                         granit_bind_group_layout* layout) {
  if (!layout)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *layout = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!desc || desc->struct_size < GRANIT_BIND_GROUP_LAYOUT_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->entry_count > 64 || (desc->entry_count != 0 && !desc->entries))
    return GRANIT_ERROR_INVALID_ARGUMENT;
#ifdef __EMSCRIPTEN__
  return GRANIT_ERROR_UNSUPPORTED;
#else
  constexpr auto valid_stages = GRANIT_SHADER_STAGE_VERTEX_BIT | GRANIT_SHADER_STAGE_FRAGMENT_BIT |
                                GRANIT_SHADER_STAGE_COMPUTE_BIT;
  for (uint32_t index = 0; index < desc->entry_count; ++index) {
    const auto& entry = desc->entries[index];
    if (entry.type < GRANIT_BINDING_TYPE_UNIFORM_BUFFER ||
        entry.type > GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER || entry.array_count == 0 ||
        entry.visibility == 0 || (entry.visibility & ~valid_stages) != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (entry.type == GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER && entry.array_count != 1)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    for (uint32_t previous = 0; previous < index; ++previous) {
      if (desc->entries[previous].binding == entry.binding)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  return granit::detail::renderer_registry::instance().create_bind_group_layout(
      renderer, {desc->entries, desc->entry_count}, *layout);
#endif
}

extern "C" granit_result granit_bind_group_layout_destroy(granit_renderer renderer,
                                                          granit_bind_group_layout layout) {
  if (renderer == GRANIT_NULL_HANDLE || layout == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
#ifdef __EMSCRIPTEN__
  return GRANIT_ERROR_UNSUPPORTED;
#else
  return granit::detail::renderer_registry::instance().destroy_bind_group_layout(renderer, layout);
#endif
}

extern "C" granit_result granit_bind_group_create(granit_renderer renderer,
                                                  const granit_bind_group_desc* desc,
                                                  granit_bind_group* bind_group) {
  if (!bind_group)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *bind_group = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!desc || desc->struct_size < GRANIT_BIND_GROUP_DESC_VERSION_1_SIZE || desc->reserved != 0 ||
      desc->entry_count > 1024 || (desc->entry_count != 0 && !desc->entries))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc->layout == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  for (uint32_t index = 0; index < desc->entry_count; ++index) {
    if (desc->entries[index].resource == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_INVALID_HANDLE;
    for (uint32_t previous = 0; previous < index; ++previous) {
      if (desc->entries[previous].binding == desc->entries[index].binding &&
          desc->entries[previous].array_element == desc->entries[index].array_element)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
#ifdef __EMSCRIPTEN__
  return GRANIT_ERROR_UNSUPPORTED;
#else
  return granit::detail::renderer_registry::instance().create_bind_group(renderer, *desc,
                                                                         *bind_group);
#endif
}

extern "C" granit_result granit_bind_group_destroy(granit_renderer renderer,
                                                   granit_bind_group bind_group) {
  if (renderer == GRANIT_NULL_HANDLE || bind_group == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
#ifdef __EMSCRIPTEN__
  return GRANIT_ERROR_UNSUPPORTED;
#else
  return granit::detail::renderer_registry::instance().destroy_bind_group(renderer, bind_group);
#endif
}

extern "C" granit_result granit_pipeline_layout_create(granit_renderer renderer,
                                                       const granit_pipeline_layout_desc* desc,
                                                       granit_pipeline_layout* layout) {
  if (!layout)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *layout = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!desc || desc->struct_size < GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->bind_group_layout_count > 8 ||
      (desc->bind_group_layout_count != 0 && !desc->bind_group_layouts))
    return GRANIT_ERROR_INVALID_ARGUMENT;
#ifdef __EMSCRIPTEN__
  if (desc->bind_group_layout_count != 0)
    return GRANIT_ERROR_UNSUPPORTED;
  return granit::detail::renderer_registry::instance().create_webgpu_pipeline_layout(renderer,
                                                                                     *layout);
#else
  return granit::detail::renderer_registry::instance().create_pipeline_layout(
      renderer, {desc->bind_group_layouts, desc->bind_group_layout_count}, *layout);
#endif
}

extern "C" granit_result granit_pipeline_layout_destroy(granit_renderer renderer,
                                                        granit_pipeline_layout layout) {
  if (renderer == GRANIT_NULL_HANDLE || layout == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().destroy_pipeline_layout(renderer, layout);
}

extern "C" granit_result granit_graphics_pipeline_create(granit_renderer renderer,
                                                         const granit_graphics_pipeline_desc* desc,
                                                         granit_graphics_pipeline* pipeline) {
  if (!pipeline)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *pipeline = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!desc || desc->struct_size < GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->reserved_2 != 0 || desc->color_format_count > 8 ||
      (desc->color_format_count != 0 && !desc->color_formats) ||
      (desc->color_format_count == 0 &&
       desc->depth_stencil_format == GRANIT_TEXTURE_FORMAT_UNDEFINED) ||
      (desc->depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED &&
       (!valid_format(desc->depth_stencil_format) ||
        desc->depth_stencil_format < GRANIT_TEXTURE_FORMAT_D16_UNORM)) ||
      (desc->sample_count != 1 && desc->sample_count != 2 && desc->sample_count != 4 &&
       desc->sample_count != 8))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc->layout == GRANIT_NULL_HANDLE || desc->vertex_shader == GRANIT_NULL_HANDLE ||
      desc->fragment_shader == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  for (uint32_t index = 0; index < desc->color_format_count; ++index) {
    if (!valid_format(desc->color_formats[index]) ||
        desc->color_formats[index] >= GRANIT_TEXTURE_FORMAT_D16_UNORM)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_2_SIZE) {
    if (desc->reserved_3 != 0 || desc->vertex_buffer_layout_count > 16 ||
        (desc->vertex_buffer_layout_count != 0 && !desc->vertex_buffer_layouts))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    std::array<bool, 32> locations{};
    for (uint32_t binding = 0; binding < desc->vertex_buffer_layout_count; ++binding) {
      const auto& layout = desc->vertex_buffer_layouts[binding];
      if (layout.stride == 0 || layout.step_mode < GRANIT_VERTEX_STEP_MODE_VERTEX ||
          layout.step_mode > GRANIT_VERTEX_STEP_MODE_INSTANCE || layout.attribute_count == 0 ||
          layout.attribute_count > locations.size() || layout.reserved != 0 || !layout.attributes)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      for (uint32_t index = 0; index < layout.attribute_count; ++index) {
        const auto& attribute = layout.attributes[index];
        const auto size = vertex_format_size(attribute.format);
        if (attribute.location >= locations.size() || locations[attribute.location] || size == 0 ||
            attribute.reserved != 0 || attribute.offset >= layout.stride ||
            size > layout.stride - attribute.offset)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        locations[attribute.location] = true;
      }
    }
  }
  if (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_3_SIZE &&
      (desc->primitive.topology < GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST ||
       desc->primitive.topology > GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
       desc->primitive.front_face < GRANIT_FRONT_FACE_COUNTER_CLOCKWISE ||
       desc->primitive.front_face > GRANIT_FRONT_FACE_CLOCKWISE ||
       desc->primitive.cull_mode < GRANIT_CULL_MODE_NONE ||
       desc->primitive.cull_mode > GRANIT_CULL_MODE_FRONT_AND_BACK ||
       desc->primitive.polygon_mode < GRANIT_POLYGON_MODE_FILL ||
       desc->primitive.polygon_mode > GRANIT_POLYGON_MODE_POINT))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_4_SIZE) {
    if (desc->reserved_4 != 0 ||
        (desc->color_blend_count != 0 &&
         (desc->color_blend_count != desc->color_format_count || !desc->color_blends)))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (desc->depth &&
        (desc->depth->test_enabled > 1 || desc->depth->write_enabled > 1 ||
         desc->depth->compare < GRANIT_COMPARE_OPERATION_NEVER ||
         desc->depth->compare > GRANIT_COMPARE_OPERATION_ALWAYS || desc->depth->reserved != 0 ||
         (desc->depth_stencil_format == GRANIT_TEXTURE_FORMAT_UNDEFINED &&
          (desc->depth->test_enabled != 0 || desc->depth->write_enabled != 0))))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    for (uint32_t index = 0; index < desc->color_blend_count; ++index) {
      const auto& state = desc->color_blends[index];
      if (state.enabled > 1 || state.source_color_factor < GRANIT_BLEND_FACTOR_ZERO ||
          state.source_color_factor > GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA ||
          state.destination_color_factor < GRANIT_BLEND_FACTOR_ZERO ||
          state.destination_color_factor > GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA ||
          state.source_alpha_factor < GRANIT_BLEND_FACTOR_ZERO ||
          state.source_alpha_factor > GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA ||
          state.destination_alpha_factor < GRANIT_BLEND_FACTOR_ZERO ||
          state.destination_alpha_factor > GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA ||
          state.color_operation < GRANIT_BLEND_OPERATION_ADD ||
          state.color_operation > GRANIT_BLEND_OPERATION_MAX ||
          state.alpha_operation < GRANIT_BLEND_OPERATION_ADD ||
          state.alpha_operation > GRANIT_BLEND_OPERATION_MAX ||
          (state.write_mask & ~GRANIT_COLOR_WRITE_ALL_BITS) != 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  if (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE && desc->depth_bias &&
      (!std::isfinite(desc->depth_bias->constant_factor) ||
       !std::isfinite(desc->depth_bias->slope_factor) || !std::isfinite(desc->depth_bias->clamp) ||
       desc->depth_bias->clamp < 0.0F || desc->depth_bias->reserved != 0))
    return GRANIT_ERROR_INVALID_ARGUMENT;
#ifdef __EMSCRIPTEN__
  if (desc->color_format_count != 1 ||
      (desc->color_formats[0] != GRANIT_TEXTURE_FORMAT_RGBA8_UNORM &&
       desc->color_formats[0] != GRANIT_TEXTURE_FORMAT_BGRA8_UNORM) ||
      desc->depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED || desc->sample_count != 1 ||
      (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_2_SIZE &&
       desc->vertex_buffer_layout_count != 0) ||
      (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_4_SIZE &&
       (desc->depth != nullptr || desc->color_blend_count != 0)) ||
      (desc->struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE && desc->depth_bias))
    return GRANIT_ERROR_UNSUPPORTED;
  return granit::detail::renderer_registry::instance().create_webgpu_graphics_pipeline(
      renderer, desc->layout, desc->vertex_shader, desc->fragment_shader, desc->color_formats[0],
      *pipeline);
#else
  return granit::detail::renderer_registry::instance().create_graphics_pipeline(renderer, *desc,
                                                                                *pipeline);
#endif
}

extern "C" granit_result granit_graphics_pipeline_destroy(granit_renderer renderer,
                                                          granit_graphics_pipeline pipeline) {
  if (renderer == GRANIT_NULL_HANDLE || pipeline == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().destroy_graphics_pipeline(renderer,
                                                                                 pipeline);
}

extern "C" granit_result granit_compute_pipeline_create(granit_renderer renderer,
                                                        const granit_compute_pipeline_desc* desc,
                                                        granit_compute_pipeline* pipeline) {
  if (!pipeline)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *pipeline = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!desc || desc->struct_size < GRANIT_COMPUTE_PIPELINE_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->reserved_2 != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc->layout == GRANIT_NULL_HANDLE || desc->compute_shader == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
#ifdef __EMSCRIPTEN__
  return GRANIT_ERROR_UNSUPPORTED;
#else
  return granit::detail::renderer_registry::instance().create_compute_pipeline(renderer, *desc,
                                                                               *pipeline);
#endif
}

extern "C" granit_result granit_compute_pipeline_destroy(granit_renderer renderer,
                                                         granit_compute_pipeline pipeline) {
  if (renderer == GRANIT_NULL_HANDLE || pipeline == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
#ifdef __EMSCRIPTEN__
  return GRANIT_ERROR_UNSUPPORTED;
#else
  return granit::detail::renderer_registry::instance().destroy_compute_pipeline(renderer, pipeline);
#endif
}
