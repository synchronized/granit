// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline.h>

#include "renderer/renderer_registry.h"

namespace {

bool valid_format(granit_texture_format format) noexcept {
  return format >= GRANIT_TEXTURE_FORMAT_R8_UNORM &&
         format <= GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
}

} // namespace

extern "C" granit_result granit_bind_group_layout_create(granit_renderer renderer,
                                                         const granit_bind_group_layout_desc* desc,
                                                         granit_bind_group_layout* layout) {
  if (!layout)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *layout = GRANIT_NULL_HANDLE;
  if (!desc || desc->struct_size < GRANIT_BIND_GROUP_LAYOUT_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->entry_count > 64 || (desc->entry_count != 0 && !desc->entries))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  constexpr auto valid_stages = GRANIT_SHADER_STAGE_VERTEX_BIT | GRANIT_SHADER_STAGE_FRAGMENT_BIT |
                                GRANIT_SHADER_STAGE_COMPUTE_BIT;
  for (uint32_t index = 0; index < desc->entry_count; ++index) {
    const auto& entry = desc->entries[index];
    if (entry.type < GRANIT_BINDING_TYPE_UNIFORM_BUFFER ||
        entry.type > GRANIT_BINDING_TYPE_SAMPLER || entry.array_count == 0 ||
        entry.visibility == 0 || (entry.visibility & ~valid_stages) != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    for (uint32_t previous = 0; previous < index; ++previous) {
      if (desc->entries[previous].binding == entry.binding)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  return granit::detail::renderer_registry::instance().create_bind_group_layout(
      renderer, {desc->entries, desc->entry_count}, *layout);
}

extern "C" granit_result granit_bind_group_layout_destroy(granit_renderer renderer,
                                                          granit_bind_group_layout layout) {
  if (renderer == GRANIT_NULL_HANDLE || layout == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().destroy_bind_group_layout(renderer, layout);
}

extern "C" granit_result granit_pipeline_layout_create(granit_renderer renderer,
                                                       const granit_pipeline_layout_desc* desc,
                                                       granit_pipeline_layout* layout) {
  if (!layout)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *layout = GRANIT_NULL_HANDLE;
  if (!desc || desc->struct_size < GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->bind_group_layout_count > 8 ||
      (desc->bind_group_layout_count != 0 && !desc->bind_group_layouts))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().create_pipeline_layout(
      renderer, {desc->bind_group_layouts, desc->bind_group_layout_count}, *layout);
}

extern "C" granit_result granit_pipeline_layout_destroy(granit_renderer renderer,
                                                        granit_pipeline_layout layout) {
  if (renderer == GRANIT_NULL_HANDLE || layout == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().destroy_pipeline_layout(renderer, layout);
}

extern "C" granit_result granit_graphics_pipeline_create(granit_renderer renderer,
                                                         const granit_graphics_pipeline_desc* desc,
                                                         granit_graphics_pipeline* pipeline) {
  if (!pipeline)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *pipeline = GRANIT_NULL_HANDLE;
  if (!desc || desc->struct_size < GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->reserved_2 != 0 || desc->layout == GRANIT_NULL_HANDLE ||
      desc->vertex_shader == GRANIT_NULL_HANDLE || desc->fragment_shader == GRANIT_NULL_HANDLE ||
      desc->color_format_count > 8 || (desc->color_format_count != 0 && !desc->color_formats) ||
      (desc->color_format_count == 0 &&
       desc->depth_stencil_format == GRANIT_TEXTURE_FORMAT_UNDEFINED) ||
      (desc->depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED &&
       (!valid_format(desc->depth_stencil_format) ||
        desc->depth_stencil_format < GRANIT_TEXTURE_FORMAT_D16_UNORM)) ||
      (desc->sample_count != 1 && desc->sample_count != 2 && desc->sample_count != 4 &&
       desc->sample_count != 8))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint32_t index = 0; index < desc->color_format_count; ++index) {
    if (!valid_format(desc->color_formats[index]) ||
        desc->color_formats[index] >= GRANIT_TEXTURE_FORMAT_D16_UNORM)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return granit::detail::renderer_registry::instance().create_graphics_pipeline(renderer, *desc,
                                                                                *pipeline);
}

extern "C" granit_result granit_graphics_pipeline_destroy(granit_renderer renderer,
                                                          granit_graphics_pipeline pipeline) {
  if (renderer == GRANIT_NULL_HANDLE || pipeline == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().destroy_graphics_pipeline(renderer,
                                                                                 pipeline);
}
