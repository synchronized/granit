// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_RENDERING_H_
#define GRANIT_BACKEND_RENDERING_H_

#include <cstdint>
#include <span>

#include <granit/core/result.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/render_target.h>
#include <granit/renderer/resource_types.h>

#include "backend/contracts/access.h"
#include "backend/contracts/binding.h"
#include "backend/contracts/resources.h"

namespace granit::detail {

struct backend_color_attachment {
  backend_texture_resource* texture{};
  backend_texture_view_resource* view{};
  granit_subresource_range range{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
  granit_attachment_load_operation load_operation{};
  granit_attachment_store_operation store_operation{};
  granit_clear_color_value clear_value{};
  backend_texture_resource* resolve_texture{};
  backend_texture_view_resource* resolve_view{};
  granit_subresource_range resolve_range{};
};

struct backend_depth_stencil_attachment {
  backend_texture_resource* texture{};
  backend_texture_view_resource* view{};
  granit_subresource_range range{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
  granit_attachment_load_operation depth_load_operation{};
  granit_attachment_store_operation depth_store_operation{};
  granit_attachment_load_operation stencil_load_operation{};
  granit_attachment_store_operation stencil_store_operation{};
  granit_clear_depth_stencil_value clear_value{};
};

/** 统一命令记录器中的图形渲染能力。 */
class backend_graphics_command_renderer {
public:
  backend_graphics_command_renderer() = default;
  virtual ~backend_graphics_command_renderer() = default;
  backend_graphics_command_renderer(const backend_graphics_command_renderer&) = delete;
  backend_graphics_command_renderer& operator=(const backend_graphics_command_renderer&) = delete;

  [[nodiscard]] virtual granit_result
  bind_graphics_pipeline(backend_command_recorder_resource&,
                         backend_graphics_pipeline_resource&) noexcept {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  [[nodiscard]] virtual granit_result
  bind_graphics_groups(backend_command_recorder_resource&, backend_pipeline_layout_resource&,
                       std::uint32_t, std::span<backend_bind_group_resource* const>,
                       std::span<const std::uint32_t>, std::span<const backend_buffer_access>,
                       std::span<const backend_texture_access>) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  [[nodiscard]] virtual granit_result set_viewports(backend_command_recorder_resource&,
                                                    std::uint32_t,
                                                    std::span<const granit_viewport>) noexcept {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  [[nodiscard]] virtual granit_result set_scissors(backend_command_recorder_resource&,
                                                   std::uint32_t,
                                                   std::span<const granit_scissor>) noexcept {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  [[nodiscard]] virtual granit_result bind_vertex_buffers(backend_command_recorder_resource&,
                                                          std::uint32_t,
                                                          std::span<backend_buffer_resource* const>,
                                                          std::span<const std::uint64_t>) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  [[nodiscard]] virtual granit_result bind_index_buffer(backend_command_recorder_resource&,
                                                        backend_buffer_resource&, std::uint64_t,
                                                        granit_index_type) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  [[nodiscard]] virtual granit_result draw(backend_command_recorder_resource& recorder,
                                           backend_texture_view_resource* target,
                                           backend_graphics_pipeline_resource* pipeline,
                                           std::uint32_t vertex_count, std::uint32_t instance_count,
                                           std::uint32_t first_vertex,
                                           std::uint32_t first_instance) noexcept = 0;
  [[nodiscard]] virtual granit_result draw_indexed(backend_command_recorder_resource&,
                                                   backend_texture_view_resource*,
                                                   backend_graphics_pipeline_resource*,
                                                   std::uint32_t, std::uint32_t, std::uint32_t,
                                                   std::int32_t, std::uint32_t) noexcept {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  [[nodiscard]] virtual granit_result begin_rendering(backend_command_recorder_resource&,
                                                      granit_rendering_area,
                                                      std::span<const backend_color_attachment>,
                                                      const backend_depth_stencil_attachment*,
                                                      std::uint32_t) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  [[nodiscard]] virtual granit_result end_rendering(backend_command_recorder_resource&) noexcept {
    return GRANIT_ERROR_UNSUPPORTED;
  }
};

} // namespace granit::detail

#endif
