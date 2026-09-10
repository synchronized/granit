// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <vector>

namespace granit::detail {

namespace {

webgpu_texture_aspect to_context_aspect(granit_texture_aspect aspect) noexcept {
  switch (aspect) {
  case GRANIT_TEXTURE_ASPECT_DEPTH_BIT:
    return GRANIT_WEBGPU_TEXTURE_ASPECT_DEPTH;
  case GRANIT_TEXTURE_ASPECT_STENCIL_BIT:
    return GRANIT_WEBGPU_TEXTURE_ASPECT_STENCIL;
  default:
    return GRANIT_WEBGPU_TEXTURE_ASPECT_ALL;
  }
}

} // namespace

std::unique_ptr<backend_command_recorder_resource>
webgpu_renderer_state::allocate_command_recorder_resource() {
  return command_owner_ ? command_allocate_recorder() : nullptr;
}

granit_result
webgpu_renderer_state::bind_compute_pipeline(backend_command_recorder_resource& recorder,
                                             backend_compute_pipeline_resource& pipeline) noexcept {
  return command_owner_ && pipeline_owner_
             ? command_bind_compute_pipeline(recorder, native_compute_pipeline(pipeline))
             : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::bind_compute_groups(
    backend_command_recorder_resource& recorder, backend_pipeline_layout_resource& layout,
    std::uint32_t first_group, std::span<backend_bind_group_resource* const> bind_groups,
    std::span<const std::uint32_t> dynamic_offsets, std::span<const backend_buffer_access>,
    std::span<const backend_texture_access>) {
  if (!command_owner_ || !resource_owner_ || !pipeline_owner_ || bind_groups.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<webgpu_bind_group> native_groups;
    native_groups.reserve(bind_groups.size());
    for (auto* group : bind_groups) {
      if (group == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto native = native_bind_group(*group);
      if (native == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_groups.push_back(native);
    }
    return command_bind_compute_groups(recorder, native_pipeline_layout(layout), first_group,
                                       native_groups, dynamic_offsets);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result webgpu_renderer_state::dispatch(backend_command_recorder_resource& recorder,
                                              std::uint32_t x, std::uint32_t y,
                                              std::uint32_t z) noexcept {
  return command_owner_ ? command_dispatch(recorder, x, y, z) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result
webgpu_renderer_state::create_command_recorder(backend_command_recorder_resource&) noexcept {
  return command_owner_ ? GRANIT_SUCCESS : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::begin_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return command_owner_ ? command_begin(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result
webgpu_renderer_state::end_command_recorder(backend_command_recorder_resource& recorder) noexcept {
  return command_owner_ ? command_end(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::reset_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return command_owner_ ? command_reset(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::discard_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return command_owner_ ? command_reset(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

bool webgpu_renderer_state::command_recorder_is_recording(
    backend_command_recorder_resource& recorder) noexcept {
  return command_owner_ && command_is_recording(recorder);
}

granit_result webgpu_renderer_state::bind_graphics_pipeline(
    backend_command_recorder_resource& recorder,
    backend_graphics_pipeline_resource& pipeline) noexcept {
  return command_owner_ && pipeline_owner_
             ? command_bind_pipeline(recorder, native_graphics_pipeline(pipeline))
             : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::bind_graphics_groups(
    backend_command_recorder_resource& recorder, backend_pipeline_layout_resource& layout,
    std::uint32_t first_group, std::span<backend_bind_group_resource* const> bind_groups,
    std::span<const std::uint32_t> dynamic_offsets,
    std::span<const backend_buffer_access> buffer_accesses,
    std::span<const backend_texture_access> texture_accesses) {
  static_cast<void>(buffer_accesses);
  static_cast<void>(texture_accesses);
  if (!command_owner_ || !resource_owner_ || !pipeline_owner_ || bind_groups.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<webgpu_bind_group> native_groups;
    native_groups.reserve(bind_groups.size());
    for (auto* bind_group : bind_groups) {
      if (bind_group == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto native = native_bind_group(*bind_group);
      if (native == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_groups.push_back(native);
    }
    const auto native_layout = native_pipeline_layout(layout);
    if (native_layout == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    return command_bind_graphics_groups(recorder, native_layout, first_group, native_groups,
                                        dynamic_offsets);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_renderer_state::bind_vertex_buffers(
    backend_command_recorder_resource& recorder, std::uint32_t first,
    std::span<backend_buffer_resource* const> buffers, std::span<const std::uint64_t> offsets) {
  if (!command_owner_ || !resource_owner_ || buffers.empty() || buffers.size() != offsets.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<webgpu_vertex_buffer_binding> bindings;
  try {
    bindings.reserve(buffers.size());
    for (std::size_t index = 0; index < buffers.size(); ++index) {
      const auto native = native_buffer(*buffers[index]);
      if (native == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      bindings.push_back({native, offsets[index]});
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  return command_bind_vertex_buffers(recorder, first, bindings);
}

granit_result webgpu_renderer_state::bind_index_buffer(backend_command_recorder_resource& recorder,
                                                       backend_buffer_resource& buffer,
                                                       std::uint64_t offset,
                                                       granit_index_type type) {
  if (!command_owner_ || !resource_owner_)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto native = native_buffer(buffer);
  if (native == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto format = type == GRANIT_INDEX_TYPE_UINT16 ? GRANIT_WEBGPU_INDEX_FORMAT_UINT16
                                                       : GRANIT_WEBGPU_INDEX_FORMAT_UINT32;
  return command_bind_index_buffer(recorder, native, offset, format);
}

granit_result
webgpu_renderer_state::set_viewports(backend_command_recorder_resource& recorder,
                                     std::uint32_t first,
                                     std::span<const granit_viewport> viewports) noexcept {
  if (!command_owner_ || viewports.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<webgpu_viewport> native;
    native.reserve(viewports.size());
    for (const auto& viewport : viewports) {
      native.push_back({viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth,
                        viewport.max_depth});
    }
    return command_set_viewports(recorder, first, native);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_renderer_state::set_scissors(backend_command_recorder_resource& recorder,
                                    std::uint32_t first,
                                    std::span<const granit_scissor> scissors) noexcept {
  if (!command_owner_ || scissors.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<webgpu_scissor> native;
    native.reserve(scissors.size());
    for (const auto& scissor : scissors) {
      native.push_back({static_cast<std::uint32_t>(scissor.x),
                        static_cast<std::uint32_t>(scissor.y), scissor.width, scissor.height});
    }
    return command_set_scissors(recorder, first, native);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_renderer_state::copy_buffer(
    backend_command_recorder_resource& recorder, backend_buffer_resource& source,
    backend_buffer_resource& destination, std::span<const granit_buffer_copy_region> regions) {
  if (!command_owner_ || !resource_owner_)
    return GRANIT_ERROR_NOT_READY;
  try {
    std::vector<webgpu_buffer_copy_region> native;
    native.reserve(regions.size());
    for (const auto& region : regions)
      native.push_back({region.source_offset, region.destination_offset, region.size});
    return command_copy_buffer(recorder, native_buffer(source), native_buffer(destination), native);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_renderer_state::copy_texture_to_buffer(
    backend_command_recorder_resource& recorder, backend_texture_resource& source,
    backend_buffer_resource& destination, granit_texture_format format,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) {
  if (!command_owner_ || !resource_owner_)
    return GRANIT_ERROR_NOT_READY;
  const auto block = texture_format_block(format);
  if (block.bytes == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  const webgpu_texture_buffer_copy native{
      layout.offset,
      layout.bytes_per_row == 0 ? ((region.width + block.width - 1) / block.width) * block.bytes
                                : layout.bytes_per_row,
      layout.rows_per_image == 0 ? (region.height + block.height - 1) / block.height
                                 : layout.rows_per_image,
      region.mip_level,
      region.base_array_layer,
      region.array_layer_count,
      to_context_aspect(region.aspect),
      region.x,
      region.y,
      region.z,
      region.width,
      region.height,
      region.depth};
  return command_copy_texture_to_buffer(recorder, native_texture(source),
                                        native_buffer(destination), native);
}

granit_result webgpu_renderer_state::copy_buffer_to_texture(
    backend_command_recorder_resource& recorder, backend_buffer_resource& source,
    backend_texture_resource& destination, granit_texture_format format,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) {
  if (!command_owner_ || !resource_owner_)
    return GRANIT_ERROR_NOT_READY;
  const auto block = texture_format_block(format);
  if (block.bytes == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  const webgpu_texture_buffer_copy native{
      layout.offset,
      layout.bytes_per_row == 0 ? ((region.width + block.width - 1) / block.width) * block.bytes
                                : layout.bytes_per_row,
      layout.rows_per_image == 0 ? (region.height + block.height - 1) / block.height
                                 : layout.rows_per_image,
      region.mip_level,
      region.base_array_layer,
      region.array_layer_count,
      to_context_aspect(region.aspect),
      region.x,
      region.y,
      region.z,
      region.width,
      region.height,
      region.depth};
  return command_copy_buffer_to_texture(recorder, native_buffer(source),
                                        native_texture(destination), native);
}

granit_result webgpu_renderer_state::copy_texture(backend_command_recorder_resource& recorder,
                                                  backend_texture_resource& source,
                                                  backend_texture_resource& destination,
                                                  const granit_texture_copy_region& region) {
  if (!command_owner_ || !resource_owner_)
    return GRANIT_ERROR_NOT_READY;
  const webgpu_texture_copy_region native{
      region.source_mip_level,
      region.source_base_array_layer,
      region.destination_mip_level,
      region.destination_base_array_layer,
      region.array_layer_count,
      to_context_aspect(static_cast<granit_texture_aspect>(region.aspect)),
      region.source_x,
      region.source_y,
      region.source_z,
      region.destination_x,
      region.destination_y,
      region.destination_z,
      region.width,
      region.height,
      region.depth};
  return command_copy_texture(recorder, native_texture(source), native_texture(destination),
                              native);
}

bool webgpu_renderer_state::texture_supports_linear_blit(
    granit_texture_format format) const noexcept {
  return format == GRANIT_TEXTURE_FORMAT_R8_UNORM || format == GRANIT_TEXTURE_FORMAT_RG8_UNORM ||
         format == GRANIT_TEXTURE_FORMAT_RGBA8_UNORM ||
         format == GRANIT_TEXTURE_FORMAT_RGBA8_SRGB ||
         format == GRANIT_TEXTURE_FORMAT_BGRA8_UNORM ||
         format == GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT;
}

granit_result webgpu_renderer_state::generate_mipmaps(backend_command_recorder_resource& recorder,
                                                      backend_texture_resource& texture,
                                                      const granit_texture_desc&,
                                                      const granit_texture_mipmap_range& range) {
  if (!command_owner_ || !resource_owner_)
    return GRANIT_ERROR_NOT_READY;
  const webgpu_texture_mipmap_range native{range.base_mip_level, range.level_count,
                                           range.base_array_layer, range.array_layer_count};
  return command_generate_mipmaps(recorder, native_texture(texture), native);
}

granit_result webgpu_renderer_state::fill_buffer(backend_command_recorder_resource& recorder,
                                                 backend_buffer_resource& buffer,
                                                 std::uint64_t offset, std::uint64_t size,
                                                 std::uint32_t value) {
  if (!command_owner_ || !resource_owner_)
    return GRANIT_ERROR_NOT_READY;
  return command_fill_buffer(recorder, native_buffer(buffer), offset, size, value);
}

granit_result webgpu_renderer_state::draw(backend_command_recorder_resource& recorder,
                                          backend_texture_view_resource*,
                                          backend_graphics_pipeline_resource*,
                                          std::uint32_t vertex_count, std::uint32_t instance_count,
                                          std::uint32_t first_vertex,
                                          std::uint32_t first_instance) noexcept {
  if (!command_owner_)
    return GRANIT_ERROR_UNSUPPORTED;
  return command_draw(recorder, vertex_count, instance_count, first_vertex, first_instance);
}

granit_result webgpu_renderer_state::draw_indexed(
    backend_command_recorder_resource& recorder, backend_texture_view_resource*,
    backend_graphics_pipeline_resource*, std::uint32_t index_count, std::uint32_t instance_count,
    std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) noexcept {
  if (!command_owner_)
    return GRANIT_ERROR_UNSUPPORTED;
  return command_draw_indexed(recorder, index_count, instance_count, first_index, vertex_offset,
                              first_instance);
}

granit_result webgpu_renderer_state::begin_rendering(
    backend_command_recorder_resource& recorder, granit_rendering_area,
    std::span<const backend_color_attachment> color_attachments,
    const backend_depth_stencil_attachment* depth_stencil_attachment, std::uint32_t layer_count) {
  if (!command_owner_ || !presentation_owner_ || color_attachments.size() > 1 || layer_count != 1 ||
      (color_attachments.empty() && depth_stencil_attachment == nullptr))
    return GRANIT_ERROR_UNSUPPORTED;
  auto load = GRANIT_WEBGPU_LOAD_OPERATION_CLEAR;
  auto store = GRANIT_WEBGPU_STORE_OPERATION_DISCARD;
  float clear[]{0.0F, 0.0F, 0.0F, 0.0F};
  webgpu_texture_view native_view{};
  webgpu_texture_view native_resolve_view{};
  if (!color_attachments.empty()) {
    const auto& attachment = color_attachments.front();
    if (attachment.load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD)
      return GRANIT_ERROR_UNSUPPORTED;
    load = attachment.load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD
               ? GRANIT_WEBGPU_LOAD_OPERATION_LOAD
               : GRANIT_WEBGPU_LOAD_OPERATION_CLEAR;
    store = attachment.store_operation == GRANIT_ATTACHMENT_STORE_OPERATION_STORE
                ? GRANIT_WEBGPU_STORE_OPERATION_STORE
                : GRANIT_WEBGPU_STORE_OPERATION_DISCARD;
    clear[0] = attachment.clear_value.red;
    clear[1] = attachment.clear_value.green;
    clear[2] = attachment.clear_value.blue;
    clear[3] = attachment.clear_value.alpha;
    native_view = native_texture_view(*attachment.view);
    if (native_view == 0)
      native_view = presentation_native_view(*attachment.view);
    if (attachment.resolve_view != nullptr) {
      native_resolve_view = native_texture_view(*attachment.resolve_view);
      if (native_resolve_view == 0)
        native_resolve_view = presentation_native_view(*attachment.resolve_view);
      if (native_resolve_view == 0)
        return GRANIT_ERROR_INVALID_HANDLE;
    }
  }
  webgpu_texture_view native_depth_view{};
  auto depth_load = GRANIT_WEBGPU_LOAD_OPERATION_CLEAR;
  auto depth_store = GRANIT_WEBGPU_STORE_OPERATION_DISCARD;
  float clear_depth = 1.0F;
  if (depth_stencil_attachment != nullptr) {
    const auto& depth = *depth_stencil_attachment;
    if (depth.format != GRANIT_TEXTURE_FORMAT_D32_FLOAT ||
        depth.depth_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD ||
        depth.stencil_load_operation != GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD ||
        depth.stencil_store_operation != GRANIT_ATTACHMENT_STORE_OPERATION_DISCARD) {
      return GRANIT_ERROR_UNSUPPORTED;
    }
    native_depth_view = native_texture_view(*depth.view);
    if (native_depth_view == 0)
      return GRANIT_ERROR_INVALID_HANDLE;
    depth_load = depth.depth_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD
                     ? GRANIT_WEBGPU_LOAD_OPERATION_LOAD
                     : GRANIT_WEBGPU_LOAD_OPERATION_CLEAR;
    depth_store = depth.depth_store_operation == GRANIT_ATTACHMENT_STORE_OPERATION_STORE
                      ? GRANIT_WEBGPU_STORE_OPERATION_STORE
                      : GRANIT_WEBGPU_STORE_OPERATION_DISCARD;
    clear_depth = depth.clear_value.depth;
  }
  return command_begin_rendering(recorder, native_view, native_resolve_view, load, store, clear,
                                 native_depth_view, depth_load, depth_store, clear_depth);
}

granit_result
webgpu_renderer_state::end_rendering(backend_command_recorder_resource& recorder) noexcept {
  return command_owner_ ? command_end_rendering(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

} // namespace granit::detail
