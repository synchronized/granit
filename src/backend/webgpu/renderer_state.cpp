// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include <new>
#include <string>
#include <vector>

namespace granit::detail {
namespace {

std::uint32_t to_plugin_surface_types(std::uint32_t surface_types) noexcept {
  std::uint32_t result{};
  if ((surface_types & GRANIT_SURFACE_TYPE_WIN32_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WIN32_BIT;
  if ((surface_types & GRANIT_SURFACE_TYPE_XCB_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_XCB_BIT;
  if ((surface_types & GRANIT_SURFACE_TYPE_WAYLAND_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WAYLAND_BIT;
  if ((surface_types & GRANIT_SURFACE_TYPE_CANVAS_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_CANVAS_BIT;
  return result;
}

} // namespace

webgpu_renderer_state::~webgpu_renderer_state() {
  presentation_.reset();
  resources_.reset();
  commands_.reset();
  pipelines_.reset();
  shaders_.reset();
  if (instance_ != 0) {
    static_cast<void>(loader_.destroy_instance(instance_));
    instance_ = 0;
  }
  loader_.close();
}

std::unique_ptr<backend_command_recorder_resource>
webgpu_renderer_state::allocate_command_recorder_resource() {
  return commands_ ? commands_->allocate_recorder() : nullptr;
}

std::unique_ptr<backend_buffer_resource> webgpu_renderer_state::allocate_buffer_resource() {
  return resources_ ? resources_->allocate_buffer() : nullptr;
}

granit_result webgpu_renderer_state::create_buffer(const granit_buffer_desc& desc,
                                                   backend_buffer_resource& buffer) noexcept {
  return resources_ ? resources_->create_buffer(desc, buffer) : GRANIT_ERROR_NOT_READY;
}

void* webgpu_renderer_state::mapped_buffer_data(backend_buffer_resource& buffer) noexcept {
  return resources_ ? resources_->mapped_data(buffer) : nullptr;
}

granit_result webgpu_renderer_state::flush_buffer(backend_buffer_resource& buffer,
                                                  std::uint64_t offset,
                                                  std::uint64_t size) noexcept {
  return resources_ ? resources_->flush(buffer, offset, size) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::invalidate_buffer(backend_buffer_resource& buffer,
                                                       std::uint64_t offset,
                                                       std::uint64_t size) noexcept {
  return resources_ ? resources_->invalidate(buffer, offset, size) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::upload_buffer(backend_buffer_resource& buffer,
                                                   std::uint64_t offset, const void* data,
                                                   std::uint64_t size) noexcept {
  return resources_ ? resources_->upload(buffer, offset, data, size) : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_renderer_state::upload_batch(std::span<const backend_upload_operation> uploads) noexcept {
  return resources_ ? resources_->upload_batch(uploads) : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_texture_resource> webgpu_renderer_state::allocate_texture_resource() {
  return resources_ ? resources_->allocate_texture() : nullptr;
}

granit_result webgpu_renderer_state::create_texture(const granit_texture_desc& desc,
                                                    backend_texture_resource& texture) noexcept {
  return resources_ ? resources_->create_texture(desc, texture) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::upload_texture(
    backend_texture_resource& texture, granit_texture_format, const void* data, std::uint64_t size,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) noexcept {
  return resources_ ? resources_->upload_texture(texture, data, size, layout, region)
                    : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_texture_view_resource>
webgpu_renderer_state::allocate_texture_view_resource() {
  return resources_ ? resources_->allocate_texture_view() : nullptr;
}

granit_result webgpu_renderer_state::create_texture_view(
    backend_texture_resource& texture, const granit_texture_desc& texture_desc,
    const granit_texture_view_desc& desc, backend_texture_view_resource& view) noexcept {
  return resources_ ? resources_->create_texture_view(texture, texture_desc, desc, view)
                    : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_sampler_resource> webgpu_renderer_state::allocate_sampler_resource() {
  return resources_ ? resources_->allocate_sampler() : nullptr;
}

granit_result webgpu_renderer_state::create_sampler(const granit_sampler_desc& desc,
                                                    backend_sampler_resource& sampler) noexcept {
  return resources_ ? resources_->create_sampler(desc, sampler) : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_bind_group_layout_resource>
webgpu_renderer_state::allocate_bind_group_layout_resource() {
  return resources_ ? resources_->allocate_bind_group_layout() : nullptr;
}

granit_result webgpu_renderer_state::create_bind_group_layout(
    std::span<const granit_bind_group_layout_entry> entries,
    backend_bind_group_layout_resource& layout) noexcept {
  return resources_ ? resources_->create_bind_group_layout(entries, layout)
                    : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_bind_group_resource> webgpu_renderer_state::allocate_bind_group_resource() {
  return resources_ ? resources_->allocate_bind_group() : nullptr;
}

granit_result
webgpu_renderer_state::create_bind_group(backend_bind_group_layout_resource& layout,
                                         std::span<const backend_bind_group_write> writes,
                                         backend_bind_group_resource& group) noexcept {
  return resources_ ? resources_->create_bind_group(layout, writes, group) : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_compute_pipeline_resource>
webgpu_renderer_state::allocate_compute_pipeline_resource() {
  return pipelines_ ? pipelines_->allocate_compute_pipeline() : nullptr;
}

granit_result webgpu_renderer_state::create_compute_pipeline(
    backend_pipeline_layout_resource& layout, backend_shader_resource& shader, const char*,
    backend_compute_pipeline_resource& pipeline) noexcept {
  if (!pipelines_ || !shaders_)
    return GRANIT_ERROR_UNSUPPORTED;
  return pipelines_->create_compute_pipeline(pipeline, pipelines_->native_pipeline_layout(layout),
                                             shaders_->native_handle(shader));
}

granit_result
webgpu_renderer_state::bind_compute_pipeline(backend_command_recorder_resource& recorder,
                                             backend_compute_pipeline_resource& pipeline) noexcept {
  return commands_ && pipelines_ ? commands_->bind_compute_pipeline(
                                       recorder, pipelines_->native_compute_pipeline(pipeline))
                                 : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::bind_compute_groups(
    backend_command_recorder_resource& recorder, backend_pipeline_layout_resource& layout,
    std::uint32_t first_group, std::span<backend_bind_group_resource* const> bind_groups,
    std::span<const std::uint32_t> dynamic_offsets, std::span<const backend_buffer_access>,
    std::span<const backend_texture_access>) {
  if (!commands_ || !resources_ || !pipelines_ || bind_groups.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_backend_plugin_bind_group> native_groups;
    native_groups.reserve(bind_groups.size());
    for (auto* group : bind_groups) {
      if (group == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto native = resources_->native_bind_group(*group);
      if (native == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_groups.push_back(native);
    }
    return commands_->bind_compute_groups(recorder, pipelines_->native_pipeline_layout(layout),
                                          first_group, native_groups, dynamic_offsets);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result webgpu_renderer_state::dispatch(backend_command_recorder_resource& recorder,
                                              std::uint32_t x, std::uint32_t y,
                                              std::uint32_t z) noexcept {
  return commands_ ? commands_->dispatch(recorder, x, y, z) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result
webgpu_renderer_state::create_command_recorder(backend_command_recorder_resource&) noexcept {
  return commands_ ? GRANIT_SUCCESS : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::begin_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->begin(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result
webgpu_renderer_state::end_command_recorder(backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->end(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::reset_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->reset(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::discard_command_recorder(
    backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->reset(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

bool webgpu_renderer_state::command_recorder_is_recording(
    backend_command_recorder_resource& recorder) noexcept {
  return commands_ && commands_->is_recording(recorder);
}

granit_result webgpu_renderer_state::bind_graphics_pipeline(
    backend_command_recorder_resource& recorder,
    backend_graphics_pipeline_resource& pipeline) noexcept {
  return commands_ && pipelines_
             ? commands_->bind_pipeline(recorder, pipelines_->native_handle(pipeline))
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
  if (!commands_ || !resources_ || !pipelines_ || bind_groups.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_backend_plugin_bind_group> native_groups;
    native_groups.reserve(bind_groups.size());
    for (auto* bind_group : bind_groups) {
      if (bind_group == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto native = resources_->native_bind_group(*bind_group);
      if (native == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_groups.push_back(native);
    }
    const auto native_layout = pipelines_->native_pipeline_layout(layout);
    if (native_layout == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    return commands_->bind_graphics_groups(recorder, native_layout, first_group, native_groups,
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
  if (!commands_ || !resources_ || buffers.empty() || buffers.size() != offsets.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<granit_backend_plugin_vertex_buffer_binding> bindings;
  try {
    bindings.reserve(buffers.size());
    for (std::size_t index = 0; index < buffers.size(); ++index) {
      const auto native = resources_->native_buffer(*buffers[index]);
      if (native == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      bindings.push_back({native, offsets[index]});
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  return commands_->bind_vertex_buffers(recorder, first, bindings);
}

granit_result webgpu_renderer_state::bind_index_buffer(backend_command_recorder_resource& recorder,
                                                       backend_buffer_resource& buffer,
                                                       std::uint64_t offset,
                                                       granit_index_type type) {
  if (!commands_ || !resources_)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto native = resources_->native_buffer(buffer);
  if (native == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto format = type == GRANIT_INDEX_TYPE_UINT16 ? GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT16
                                                       : GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT32;
  return commands_->bind_index_buffer(recorder, native, offset, format);
}

granit_result
webgpu_renderer_state::set_viewports(backend_command_recorder_resource& recorder,
                                     std::uint32_t first,
                                     std::span<const granit_viewport> viewports) noexcept {
  if (!commands_ || viewports.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_backend_plugin_viewport> native;
    native.reserve(viewports.size());
    for (const auto& viewport : viewports) {
      native.push_back({viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth,
                        viewport.max_depth});
    }
    return commands_->set_viewports(recorder, first, native);
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
  if (!commands_ || scissors.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_backend_plugin_scissor> native;
    native.reserve(scissors.size());
    for (const auto& scissor : scissors) {
      native.push_back({static_cast<std::uint32_t>(scissor.x),
                        static_cast<std::uint32_t>(scissor.y), scissor.width, scissor.height});
    }
    return commands_->set_scissors(recorder, first, native);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_renderer_state::copy_buffer(backend_command_recorder_resource&,
                                                 backend_buffer_resource&, backend_buffer_resource&,
                                                 std::span<const granit_buffer_copy_region>) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::copy_texture_to_buffer(
    backend_command_recorder_resource& recorder, backend_texture_resource& source,
    backend_buffer_resource& destination, granit_texture_format format,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) {
  if (!commands_ || !resources_ || format != GRANIT_TEXTURE_FORMAT_RGBA8_UNORM ||
      region.mip_level != 0 || region.base_array_layer != 0 || region.array_layer_count != 1 ||
      region.x != 0 || region.y != 0 || region.z != 0 || region.depth != 1)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto texture = resources_->native_texture(source);
  const auto buffer = resources_->native_buffer(destination);
  const auto bytes_per_row = layout.bytes_per_row == 0 ? region.width * 4 : layout.bytes_per_row;
  return commands_->copy_texture_to_buffer(recorder, texture, buffer, region.width, region.height,
                                           bytes_per_row);
}

granit_result webgpu_renderer_state::copy_buffer_to_texture(
    backend_command_recorder_resource&, backend_buffer_resource&, backend_texture_resource&,
    granit_texture_format, const granit_texture_data_layout&, const granit_texture_write_region&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::copy_texture(backend_command_recorder_resource&,
                                                  backend_texture_resource&,
                                                  backend_texture_resource&,
                                                  const granit_texture_copy_region&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

bool webgpu_renderer_state::texture_supports_linear_blit(granit_texture_format) const noexcept {
  return false;
}

granit_result webgpu_renderer_state::generate_mipmaps(backend_command_recorder_resource&,
                                                      backend_texture_resource&,
                                                      const granit_texture_desc&,
                                                      const granit_texture_mipmap_range&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::fill_buffer(backend_command_recorder_resource&,
                                                 backend_buffer_resource&, std::uint64_t,
                                                 std::uint64_t, std::uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::draw(backend_command_recorder_resource& recorder,
                                          backend_texture_view_resource*,
                                          backend_graphics_pipeline_resource*,
                                          std::uint32_t vertex_count, std::uint32_t instance_count,
                                          std::uint32_t first_vertex,
                                          std::uint32_t first_instance) noexcept {
  if (!commands_)
    return GRANIT_ERROR_UNSUPPORTED;
  return commands_->draw(recorder, vertex_count, instance_count, first_vertex, first_instance);
}

granit_result webgpu_renderer_state::draw_indexed(
    backend_command_recorder_resource& recorder, backend_texture_view_resource*,
    backend_graphics_pipeline_resource*, std::uint32_t index_count, std::uint32_t instance_count,
    std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) noexcept {
  if (!commands_)
    return GRANIT_ERROR_UNSUPPORTED;
  return commands_->draw_indexed(recorder, index_count, instance_count, first_index, vertex_offset,
                                 first_instance);
}

granit_result webgpu_renderer_state::begin_rendering(
    backend_command_recorder_resource& recorder, granit_rendering_area,
    std::span<const backend_color_attachment> color_attachments,
    const backend_depth_stencil_attachment* depth_stencil_attachment, std::uint32_t layer_count) {
  if (!commands_ || !presentation_ || color_attachments.size() > 1 || layer_count != 1 ||
      (color_attachments.empty() && depth_stencil_attachment == nullptr))
    return GRANIT_ERROR_UNSUPPORTED;
  auto load = GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR;
  auto store = GRANIT_BACKEND_PLUGIN_STORE_OPERATION_DISCARD;
  float clear[]{0.0F, 0.0F, 0.0F, 0.0F};
  granit_backend_plugin_texture_view native_view{};
  if (!color_attachments.empty()) {
    const auto& attachment = color_attachments.front();
    if (attachment.load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD)
      return GRANIT_ERROR_UNSUPPORTED;
    load = attachment.load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD
               ? GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_LOAD
               : GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR;
    store = attachment.store_operation == GRANIT_ATTACHMENT_STORE_OPERATION_STORE
                ? GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE
                : GRANIT_BACKEND_PLUGIN_STORE_OPERATION_DISCARD;
    clear[0] = attachment.clear_value.red;
    clear[1] = attachment.clear_value.green;
    clear[2] = attachment.clear_value.blue;
    clear[3] = attachment.clear_value.alpha;
    native_view = resources_->native_texture_view(*attachment.view);
    if (native_view == 0)
      native_view = presentation_->native_view(*attachment.view);
  }
  granit_backend_plugin_texture_view native_depth_view{};
  auto depth_load = GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR;
  auto depth_store = GRANIT_BACKEND_PLUGIN_STORE_OPERATION_DISCARD;
  float clear_depth = 1.0F;
  if (depth_stencil_attachment != nullptr) {
    const auto& depth = *depth_stencil_attachment;
    if (depth.format != GRANIT_TEXTURE_FORMAT_D32_FLOAT ||
        depth.depth_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD ||
        depth.stencil_load_operation != GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD ||
        depth.stencil_store_operation != GRANIT_ATTACHMENT_STORE_OPERATION_DISCARD) {
      return GRANIT_ERROR_UNSUPPORTED;
    }
    native_depth_view = resources_->native_texture_view(*depth.view);
    if (native_depth_view == 0)
      return GRANIT_ERROR_INVALID_HANDLE;
    depth_load = depth.depth_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD
                     ? GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_LOAD
                     : GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR;
    depth_store = depth.depth_store_operation == GRANIT_ATTACHMENT_STORE_OPERATION_STORE
                      ? GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE
                      : GRANIT_BACKEND_PLUGIN_STORE_OPERATION_DISCARD;
    clear_depth = depth.clear_value.depth;
  }
  return commands_->begin_rendering(recorder, native_view, load, store, clear, native_depth_view,
                                    depth_load, depth_store, clear_depth);
}

granit_result
webgpu_renderer_state::end_rendering(backend_command_recorder_resource& recorder) noexcept {
  return commands_ ? commands_->end_rendering(recorder) : GRANIT_ERROR_UNSUPPORTED;
}

std::unique_ptr<backend_shader_resource> webgpu_renderer_state::allocate_shader_resource() {
  return shaders_ ? shaders_->allocate_shader() : nullptr;
}

granit_result webgpu_renderer_state::create_wgsl_shader(backend_shader_resource& shader,
                                                        granit_shader_stage stage,
                                                        std::string_view source,
                                                        std::string_view entry_point) noexcept {
  return shaders_ ? shaders_->create_shader(shader, stage, source.data(), source.size(),
                                            entry_point.data(), entry_point.size())
                  : GRANIT_ERROR_UNSUPPORTED;
}

std::unique_ptr<backend_pipeline_layout_resource>
webgpu_renderer_state::allocate_pipeline_layout_resource() {
  return pipelines_ ? pipelines_->allocate_pipeline_layout() : nullptr;
}

granit_result webgpu_renderer_state::create_pipeline_layout(
    std::span<backend_bind_group_layout_resource* const> bind_group_layouts,
    backend_pipeline_layout_resource& layout) noexcept {
  if (!pipelines_ || !resources_)
    return GRANIT_ERROR_UNSUPPORTED;
  try {
    std::vector<granit_backend_plugin_bind_group_layout> native_layouts;
    native_layouts.reserve(bind_group_layouts.size());
    for (auto* bind_group_layout : bind_group_layouts) {
      if (bind_group_layout == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto native = resources_->native_bind_group_layout(*bind_group_layout);
      if (native == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_layouts.push_back(native);
    }
    return pipelines_->create_pipeline_layout(native_layouts, layout);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

std::unique_ptr<backend_graphics_pipeline_resource>
webgpu_renderer_state::allocate_graphics_pipeline_resource() {
  return pipelines_ ? pipelines_->allocate_graphics_pipeline() : nullptr;
}

granit_result webgpu_renderer_state::validate_graphics_pipeline(
    const granit_graphics_pipeline_desc& desc) const noexcept {
  return pipelines_ ? pipelines_->validate_graphics_pipeline(desc) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result webgpu_renderer_state::create_graphics_pipeline(
    const backend_graphics_pipeline_create_info& info,
    backend_graphics_pipeline_resource& pipeline) noexcept {
  if (!pipelines_ || !shaders_ || info.color_formats.size() > 1)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto color_format =
      info.color_formats.empty() ? GRANIT_TEXTURE_FORMAT_UNDEFINED : info.color_formats.front();
  return pipelines_->create_graphics_pipeline(
      pipeline, info.layout, shaders_->native_handle(info.vertex_shader),
      shaders_->native_handle(info.fragment_shader), info.vertex_buffers, color_format,
      info.depth_stencil_format, info.depth, info.depth_bias);
}

void* webgpu_renderer_state::allocate(std::uint64_t size, std::uint64_t alignment, void*) noexcept {
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void webgpu_renderer_state::deallocate(void* memory, std::uint64_t, std::uint64_t alignment,
                                       void*) noexcept {
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void webgpu_renderer_state::diagnose(granit_diagnostic_severity severity,
                                     granit_diagnostic_category category, const char* message,
                                     std::uint32_t message_length, void* user_data) noexcept {
  if (user_data == nullptr || (message == nullptr && message_length != 0)) {
    return;
  }
  auto& state = *static_cast<webgpu_renderer_state*>(user_data);
  if (state.diagnostic_callback_ == nullptr) {
    return;
  }
  try {
    state.diagnostic_callback_(severity, category, message, message_length,
                               state.diagnostic_user_data_);
  } catch (...) {
  }
}

granit_result webgpu_renderer_state::initialize_static(
    const granit_backend_plugin_api* api, std::uint32_t surface_types,
    granit_diagnostic_callback diagnostic_callback, void* diagnostic_user_data) noexcept {
  if (instance_ != 0 || loader_.is_open()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  diagnostic_callback_ = diagnostic_callback;
  diagnostic_user_data_ = diagnostic_user_data;
  surface_types_ = surface_types;
  auto result = loader_.open_static(api, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU);
  if (result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, result};
    return result;
  }
  return finish_initialization();
}

granit_result webgpu_renderer_state::initialize_dynamic(
    std::string_view library_path, std::uint32_t surface_types,
    granit_diagnostic_callback diagnostic_callback, void* diagnostic_user_data) noexcept {
  if (instance_ != 0 || loader_.is_open() || library_path.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  diagnostic_callback_ = diagnostic_callback;
  diagnostic_user_data_ = diagnostic_user_data;
  surface_types_ = surface_types;
  try {
    const std::string path{library_path};
    const auto result = loader_.open(path.c_str(), GRANIT_BACKEND_PLUGIN_KIND_WEBGPU);
    if (result != GRANIT_SUCCESS) {
      lifecycle_ = {backend_lifecycle_state::failed, result};
      return result;
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  return finish_initialization();
}

granit_result webgpu_renderer_state::finish_initialization() noexcept {
  granit_backend_plugin_host_api host{sizeof(host), 0,          diagnose, this,
                                      allocate,     deallocate, nullptr};
  auto result = loader_.create_instance(&host, &instance_);
  if (result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, result};
    loader_.close();
    return result;
  }
  const auto refresh_result = refresh_state();
  return refresh_result == GRANIT_ERROR_NOT_READY ? GRANIT_SUCCESS : refresh_result;
}

granit_result webgpu_renderer_state::process_backend_events() noexcept {
  if (instance_ == 0) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto result = loader_.process_events(instance_);
  if (result != GRANIT_SUCCESS && result != GRANIT_ERROR_NOT_READY &&
      result != GRANIT_ERROR_DEVICE_LOST) {
    return result;
  }
  const auto refresh_result = refresh_state();
  return refresh_result == GRANIT_ERROR_NOT_READY ? GRANIT_SUCCESS : refresh_result;
}

backend_lifecycle_status webgpu_renderer_state::lifecycle_status() const noexcept {
  return lifecycle_;
}

granit_result webgpu_renderer_state::refresh_state() noexcept {
  granit_backend_plugin_instance_status status{};
  status.struct_size = sizeof(status);
  const auto status_result = loader_.get_instance_status(instance_, &status);
  if (status_result != GRANIT_SUCCESS) {
    lifecycle_ = {backend_lifecycle_state::failed, status_result};
    return status_result;
  }
  switch (status.state) {
  case GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_INITIALIZING:
    lifecycle_ = {backend_lifecycle_state::initializing, GRANIT_SUCCESS};
    return GRANIT_ERROR_NOT_READY;
  case GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_FAILED:
    lifecycle_ = {backend_lifecycle_state::failed, status.failure_result};
    return status.failure_result;
  case GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_DEVICE_LOST:
    lifecycle_ = {backend_lifecycle_state::device_lost, status.failure_result};
    return status.failure_result;
  case GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_READY:
    break;
  default:
    lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_INTERNAL};
    return GRANIT_ERROR_INTERNAL;
  }

  if (presentation_ == nullptr || resources_ == nullptr || shaders_ == nullptr ||
      pipelines_ == nullptr || commands_ == nullptr) {
    granit_backend_plugin_capabilities capabilities{};
    capabilities.struct_size = sizeof(capabilities);
    const auto capabilities_result = loader_.get_capabilities(instance_, &capabilities);
    if (capabilities_result != GRANIT_SUCCESS) {
      lifecycle_ = {backend_lifecycle_state::failed, capabilities_result};
      return capabilities_result;
    }
    capabilities_ = {
        capabilities.uniform_buffer_offset_alignment,
        capabilities.storage_buffer_offset_alignment,
        capabilities.max_uniform_buffer_binding_size,
        capabilities.max_storage_buffer_binding_size,
    };
    provider_surface_types_ = capabilities.surface_types;
    if ((to_plugin_surface_types(surface_types_) & ~provider_surface_types_) != 0) {
      lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_UNSUPPORTED};
      return GRANIT_ERROR_UNSUPPORTED;
    }
    try {
      auto presentation = std::make_unique<webgpu_presentation_adapter>(loader_, instance_);
      auto resources = std::make_unique<webgpu_resource_adapter>(loader_, instance_);
      auto shaders = std::make_unique<webgpu_shader_adapter>(loader_, instance_);
      auto pipelines = std::make_unique<webgpu_pipeline_adapter>(loader_, instance_);
      auto commands = std::make_unique<webgpu_command_adapter>(loader_, instance_);
      presentation_ = std::move(presentation);
      resources_ = std::move(resources);
      shaders_ = std::move(shaders);
      pipelines_ = std::move(pipelines);
      commands_ = std::move(commands);
    } catch (const std::bad_alloc&) {
      lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_OUT_OF_MEMORY};
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      lifecycle_ = {backend_lifecycle_state::failed, GRANIT_ERROR_INTERNAL};
      return GRANIT_ERROR_INTERNAL;
    }
  }
  lifecycle_ = {backend_lifecycle_state::ready, GRANIT_SUCCESS};
  return GRANIT_SUCCESS;
}

std::unique_ptr<backend_surface_resource> webgpu_renderer_state::allocate_surface_resource() {
  return presentation_ != nullptr ? presentation_->allocate_surface() : nullptr;
}

std::unique_ptr<backend_swapchain_resource> webgpu_renderer_state::allocate_swapchain_resource() {
  return presentation_ != nullptr ? presentation_->allocate_swapchain() : nullptr;
}

granit_result
webgpu_renderer_state::create_win32_surface(void* instance, void* window,
                                            backend_surface_resource& surface) noexcept {
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WIN32_BIT) == 0 ||
      (provider_surface_types_ & GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WIN32_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  return presentation_ != nullptr ? presentation_->create_win32_surface(surface, instance, window)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_renderer_state::create_xcb_surface(void* connection, std::uint32_t window,
                                          backend_surface_resource& surface) noexcept {
  if ((surface_types_ & GRANIT_SURFACE_TYPE_XCB_BIT) == 0 ||
      (provider_surface_types_ & GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_XCB_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  return presentation_ != nullptr ? presentation_->create_xcb_surface(surface, connection, window)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_renderer_state::create_wayland_surface(void* display, void* native_surface,
                                              backend_surface_resource& surface) noexcept {
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WAYLAND_BIT) == 0 ||
      (provider_surface_types_ & GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WAYLAND_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  return presentation_ != nullptr
             ? presentation_->create_wayland_surface(surface, display, native_surface)
             : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_renderer_state::create_canvas_surface(std::string_view selector,
                                             backend_surface_resource& surface) noexcept {
  if ((surface_types_ & GRANIT_SURFACE_TYPE_CANVAS_BIT) == 0 ||
      (provider_surface_types_ & GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_CANVAS_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  if (presentation_ == nullptr)
    return GRANIT_ERROR_NOT_READY;
  return presentation_->create_canvas_surface(surface, selector.data(),
                                              static_cast<std::uint32_t>(selector.size()));
}

granit_result webgpu_renderer_state::create_swapchain(backend_surface_resource& surface,
                                                      const backend_swapchain_desc& desc,
                                                      backend_swapchain_resource& swapchain) {
  return presentation_ != nullptr ? presentation_->create_swapchain(surface, desc, swapchain)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::recreate_swapchain(backend_surface_resource&,
                                                        const backend_swapchain_desc& desc,
                                                        backend_swapchain_resource& swapchain) {
  return presentation_ != nullptr ? presentation_->recreate_swapchain(swapchain, desc)
                                  : GRANIT_ERROR_NOT_READY;
}

backend_swapchain_info
webgpu_renderer_state::get_swapchain_info(backend_swapchain_resource& swapchain) noexcept {
  backend_swapchain_info info{};
  if (presentation_ != nullptr)
    static_cast<void>(presentation_->get_swapchain_info(swapchain, info));
  return info;
}

granit_result webgpu_renderer_state::get_swapchain_backbuffers(
    backend_swapchain_resource&, std::vector<backend_swapchain_backbuffer>& backbuffers) {
  // WebGPU 的当前纹理由 Acquire 动态提供，不存在可预先枚举的固定后备缓冲。
  backbuffers.clear();
  return presentation_ != nullptr ? GRANIT_SUCCESS : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_renderer_state::prepare_swapchain_backbuffer(backend_swapchain_backbuffer& backbuffer) {
  return backbuffer.texture && backbuffer.view ? GRANIT_SUCCESS : GRANIT_ERROR_INTERNAL;
}

granit_result
webgpu_renderer_state::acquire_swapchain_frame(backend_swapchain_resource& swapchain,
                                               backend_acquired_swapchain_frame& frame) {
  return presentation_ != nullptr ? presentation_->acquire_swapchain(swapchain, frame)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::present_swapchain_frame(backend_swapchain_resource& swapchain,
                                                             std::uint32_t, std::size_t,
                                                             bool& needs_recreate) {
  return presentation_ != nullptr ? presentation_->present_swapchain(swapchain, needs_recreate)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::cancel_swapchain_frame(backend_swapchain_resource& swapchain,
                                                            std::uint32_t, std::size_t,
                                                            bool& needs_recreate) {
  return presentation_ != nullptr ? presentation_->cancel_swapchain(swapchain, needs_recreate)
                                  : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::wait_for_present_idle() noexcept {
  return lifecycle_.state == backend_lifecycle_state::device_lost ? GRANIT_ERROR_DEVICE_LOST
                                                                  : GRANIT_SUCCESS;
}

std::size_t webgpu_renderer_state::collect_present_retired() noexcept { return 0; }

std::size_t webgpu_renderer_state::frame_slot_count() const noexcept {
  // 浏览器交换链按 Acquire 返回动态纹理，当前只允许一个在途呈现帧。
  return 1;
}

granit_result
webgpu_renderer_state::submit_command_recorder(backend_command_recorder_resource& recorder,
                                               submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (commands_ == nullptr)
    return GRANIT_ERROR_NOT_READY;
  const auto result = commands_->submit(recorder);
  if (result == GRANIT_SUCCESS)
    submitted_serial = next_submission_serial_++;
  return result;
}

granit_result webgpu_renderer_state::submit_command_recorders(
    std::span<backend_command_recorder_resource* const> recorders,
    submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (recorders.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (auto* recorder : recorders) {
    if (recorder == nullptr)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto result = commands_->submit(*recorder);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  submitted_serial = next_submission_serial_++;
  return GRANIT_SUCCESS;
}

granit_result
webgpu_renderer_state::wait_command_recorder(backend_command_recorder_resource&) noexcept {
  return GRANIT_SUCCESS;
}

granit_result webgpu_renderer_state::wait_for_all_submissions() noexcept {
  return lifecycle_.state == backend_lifecycle_state::device_lost ? GRANIT_ERROR_DEVICE_LOST
                                                                  : GRANIT_SUCCESS;
}

void webgpu_renderer_state::retire_resource(submission_serial, retirement_order,
                                            std::shared_ptr<void> resource) {
  // WebGPU 命令会持有所引用对象；命令完成编码后即可释放应用侧引用。
  resource.reset();
}

std::size_t webgpu_renderer_state::collect_retired() noexcept { return 0; }

std::size_t webgpu_renderer_state::pending_retirement_count() const noexcept { return 0; }

granit_result
webgpu_renderer_state::submit_swapchain_frame(backend_command_recorder_resource& recorder,
                                              backend_swapchain_resource&, std::uint32_t,
                                              std::size_t, submission_serial& submitted_serial) {
  return submit_command_recorder(recorder, submitted_serial);
}

} // namespace granit::detail
