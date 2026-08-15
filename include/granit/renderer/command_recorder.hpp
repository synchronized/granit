// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_COMMAND_RECORDER_HPP_
#define GRANIT_COMMAND_RECORDER_HPP_

#include <array>
#include <new>
#include <span>
#include <utility>
#include <vector>

#include <granit/core/result.hpp>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/render_target.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/renderer/timestamp_query.h>

namespace granit {

using buffer_copy_region = granit_buffer_copy_region;
using texture_copy_region = granit_texture_copy_region;
using viewport = granit_viewport;
using scissor = granit_scissor;
using vertex_buffer_binding = granit_vertex_buffer_binding;

enum class index_type : std::uint32_t {
  uint16 = GRANIT_INDEX_TYPE_UINT16,
  uint32 = GRANIT_INDEX_TYPE_UINT32,
};

/** 无异常、move-only 的 Command Recorder 包装。 */
class command_recorder {
public:
  command_recorder() = default;
  ~command_recorder() { static_cast<void>(destroy()); }
  command_recorder(const command_recorder&) = delete;
  command_recorder& operator=(const command_recorder&) = delete;
  command_recorder(command_recorder&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  command_recorder& operator=(command_recorder&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer) noexcept {
    if (valid() || renderer == GRANIT_NULL_HANDLE) {
      return result::invalid_argument;
    }
    const granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
    const auto value = granit_command_recorder_create(renderer, &desc, &handle_);
    if (value == GRANIT_SUCCESS) {
      renderer_ = renderer;
    }
    return from_native(value);
  }
  [[nodiscard]] result begin() noexcept {
    return from_native(granit_command_recorder_begin(renderer_, handle_));
  }
  [[nodiscard]] result end() noexcept {
    return from_native(granit_command_recorder_end(renderer_, handle_));
  }
  [[nodiscard]] result submit() noexcept {
    return from_native(granit_command_recorder_submit(renderer_, handle_));
  }
  [[nodiscard]] static result submit_batch(std::span<command_recorder> recorders) noexcept {
    if (recorders.empty() || recorders.size() > UINT32_MAX) {
      return result::invalid_argument;
    }
    try {
      std::vector<granit_command_recorder> handles;
      handles.reserve(recorders.size());
      const auto renderer = recorders.front().renderer_;
      for (const auto& recorder : recorders) {
        if (renderer == GRANIT_NULL_HANDLE || recorder.renderer_ != renderer || !recorder.valid()) {
          return result::invalid_argument;
        }
        handles.push_back(recorder.handle_);
      }
      return from_native(granit_command_recorder_submit_batch(
          renderer, handles.data(), static_cast<std::uint32_t>(handles.size())));
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result submit(const acquired_frame& frame) noexcept {
    return from_native(granit_command_recorder_submit_frame(renderer_, handle_, frame.handle));
  }
  [[nodiscard]] result reset() noexcept {
    return from_native(granit_command_recorder_reset(renderer_, handle_));
  }
  [[nodiscard]] result copy_buffer(granit_buffer source, granit_buffer destination,
                                   std::span<const buffer_copy_region> regions) noexcept {
    if (regions.size() > UINT32_MAX) {
      return result::invalid_argument;
    }
    return from_native(
        granit_command_recorder_copy_buffer(renderer_, handle_, source, destination, regions.data(),
                                            static_cast<std::uint32_t>(regions.size())));
  }
  [[nodiscard]] result copy_texture_to_buffer(granit_texture source, granit_buffer destination,
                                              const granit_texture_data_layout& layout,
                                              const granit_texture_write_region& region) noexcept {
    return from_native(granit_command_recorder_copy_texture_to_buffer(
        renderer_, handle_, source, destination, &layout, &region));
  }
  [[nodiscard]] result copy_texture(granit_texture source, granit_texture destination,
                                    const texture_copy_region& region) noexcept {
    return from_native(
        granit_command_recorder_copy_texture(renderer_, handle_, source, destination, &region));
  }
  [[nodiscard]] result fill_buffer(granit_buffer buffer, std::uint64_t offset, std::uint64_t size,
                                   std::uint32_t value) noexcept {
    return from_native(
        granit_command_recorder_fill_buffer(renderer_, handle_, buffer, offset, size, value));
  }
  [[nodiscard]] result bind_graphics_pipeline(granit_graphics_pipeline pipeline) noexcept {
    return from_native(
        granit_command_recorder_bind_graphics_pipeline(renderer_, handle_, pipeline));
  }
  [[nodiscard]] result
  bind_graphics_groups(granit_pipeline_layout layout, std::uint32_t first_group,
                       std::span<const granit_bind_group> bind_groups) noexcept {
    if (bind_groups.empty() || bind_groups.size() > UINT32_MAX)
      return result::invalid_argument;
    return from_native(granit_command_recorder_bind_graphics_groups(
        renderer_, handle_, layout, first_group, bind_groups.data(),
        static_cast<std::uint32_t>(bind_groups.size())));
  }
  [[nodiscard]] result bind_compute_pipeline(granit_compute_pipeline pipeline) noexcept {
    return from_native(granit_command_recorder_bind_compute_pipeline(renderer_, handle_, pipeline));
  }
  [[nodiscard]] result
  bind_compute_groups(granit_pipeline_layout layout, std::uint32_t first_group,
                      std::span<const granit_bind_group> bind_groups) noexcept {
    if (bind_groups.empty() || bind_groups.size() > UINT32_MAX)
      return result::invalid_argument;
    return from_native(granit_command_recorder_bind_compute_groups(
        renderer_, handle_, layout, first_group, bind_groups.data(),
        static_cast<std::uint32_t>(bind_groups.size())));
  }
  [[nodiscard]] result dispatch(std::uint32_t group_count_x, std::uint32_t group_count_y = 1,
                                std::uint32_t group_count_z = 1) noexcept {
    return from_native(granit_command_recorder_dispatch(renderer_, handle_, group_count_x,
                                                        group_count_y, group_count_z));
  }
  [[nodiscard]] result set_viewports(std::uint32_t first,
                                     std::span<const viewport> viewports) noexcept {
    if (viewports.empty() || viewports.size() > UINT32_MAX)
      return result::invalid_argument;
    return from_native(granit_command_recorder_set_viewports(
        renderer_, handle_, first, viewports.data(), static_cast<std::uint32_t>(viewports.size())));
  }
  [[nodiscard]] result set_scissors(std::uint32_t first,
                                    std::span<const scissor> scissors) noexcept {
    if (scissors.empty() || scissors.size() > UINT32_MAX)
      return result::invalid_argument;
    return from_native(granit_command_recorder_set_scissors(
        renderer_, handle_, first, scissors.data(), static_cast<std::uint32_t>(scissors.size())));
  }
  [[nodiscard]] result
  bind_vertex_buffers(std::uint32_t first,
                      std::span<const vertex_buffer_binding> bindings) noexcept {
    if (bindings.empty() || bindings.size() > UINT32_MAX)
      return result::invalid_argument;
    return from_native(granit_command_recorder_bind_vertex_buffers(
        renderer_, handle_, first, bindings.data(), static_cast<std::uint32_t>(bindings.size())));
  }
  [[nodiscard]] result bind_index_buffer(granit_buffer buffer, std::uint64_t offset,
                                         index_type type) noexcept {
    return from_native(granit_command_recorder_bind_index_buffer(
        renderer_, handle_, buffer, offset, static_cast<granit_index_type>(type)));
  }
  [[nodiscard]] result draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1,
                            std::uint32_t first_vertex = 0,
                            std::uint32_t first_instance = 0) noexcept {
    return from_native(granit_command_recorder_draw(renderer_, handle_, vertex_count,
                                                    instance_count, first_vertex, first_instance));
  }
  [[nodiscard]] result draw_indexed(std::uint32_t index_count, std::uint32_t instance_count = 1,
                                    std::uint32_t first_index = 0, std::int32_t vertex_offset = 0,
                                    std::uint32_t first_instance = 0) noexcept {
    return from_native(granit_command_recorder_draw_indexed(renderer_, handle_, index_count,
                                                            instance_count, first_index,
                                                            vertex_offset, first_instance));
  }
  [[nodiscard]] result begin_rendering(const rendering_desc& desc) noexcept {
    if (desc.color_attachments.size() > GRANIT_MAX_COLOR_ATTACHMENTS) {
      return result::invalid_argument;
    }
    std::array<granit_color_attachment_desc, GRANIT_MAX_COLOR_ATTACHMENTS> colors{};
    for (std::size_t index = 0; index < desc.color_attachments.size(); ++index) {
      colors[index] = desc.color_attachments[index].native();
    }
    granit_depth_stencil_attachment_desc depth{};
    const granit_depth_stencil_attachment_desc* depth_pointer = nullptr;
    if (desc.depth_stencil_attachment != nullptr) {
      depth = desc.depth_stencil_attachment->native();
      depth_pointer = &depth;
    }
    const granit_rendering_desc native{
        .struct_size = GRANIT_RENDERING_DESC_VERSION_1_SIZE,
        .color_attachment_count = static_cast<std::uint32_t>(desc.color_attachments.size()),
        .color_attachments = colors.data(),
        .depth_stencil_attachment = depth_pointer,
        .area = {desc.area.x, desc.area.y, desc.area.width, desc.area.height},
        .layer_count = desc.layer_count,
        .reserved = 0,
        .reserved_2 = 0,
    };
    return from_native(granit_command_recorder_begin_rendering(renderer_, handle_, &native));
  }
  [[nodiscard]] result end_rendering() noexcept {
    return from_native(granit_command_recorder_end_rendering(renderer_, handle_));
  }
  [[nodiscard]] result reset_timestamp_queries(granit_timestamp_query_pool pool,
                                               std::uint32_t first, std::uint32_t count) noexcept {
    return from_native(
        granit_command_recorder_reset_timestamp_queries(renderer_, handle_, pool, first, count));
  }
  [[nodiscard]] result write_timestamp(granit_timestamp_query_pool pool,
                                       granit_timestamp_stage stage, std::uint32_t index) noexcept {
    return from_native(
        granit_command_recorder_write_timestamp(renderer_, handle_, pool, stage, index));
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_command_recorder_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_command_recorder native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_command_recorder handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
