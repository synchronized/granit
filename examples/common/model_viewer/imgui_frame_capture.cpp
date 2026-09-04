// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/imgui_frame_capture.h"

#include <algorithm>
#include <limits>
#include <new>

namespace granit::example::model_viewer {
namespace {

std::uint32_t premultiply_color(std::uint32_t color) noexcept {
  const auto alpha = (color >> 24U) & 0xffU;
  const auto premultiply = [alpha](std::uint32_t channel) {
    return (channel * alpha + 127U) / 255U;
  };
  return (alpha << 24U) | (premultiply((color >> 16U) & 0xffU) << 16U) |
         (premultiply((color >> 8U) & 0xffU) << 8U) | premultiply(color & 0xffU);
}

} // namespace

granit::result capture_imgui_frame(const ImDrawData* draw_data,
                                   granit::integration::imgui::texture_resolver resolver,
                                   void* user_data, frame_canvas_data& output) noexcept {
  output.clear();
  if (draw_data == nullptr || resolver == nullptr)
    return granit::result::invalid_argument;
  if (!draw_data->Valid || draw_data->CmdLists.Size == 0)
    return granit::result::success;
  const auto framebuffer_width = draw_data->DisplaySize.x * draw_data->FramebufferScale.x;
  const auto framebuffer_height = draw_data->DisplaySize.y * draw_data->FramebufferScale.y;
  if (framebuffer_width <= 0.0F || framebuffer_height <= 0.0F ||
      framebuffer_width > static_cast<float>(std::numeric_limits<std::int32_t>::max()) ||
      framebuffer_height > static_cast<float>(std::numeric_limits<std::int32_t>::max())) {
    return granit::result::invalid_argument;
  }
  try {
    output.vertices.reserve(static_cast<std::size_t>(draw_data->TotalVtxCount));
    output.indices.reserve(static_cast<std::size_t>(draw_data->TotalIdxCount));
    for (const auto* source : draw_data->CmdLists) {
      if (source->VtxBuffer.Size < 0 || source->IdxBuffer.Size < 0 ||
          static_cast<std::size_t>(source->VtxBuffer.Size) >
              std::numeric_limits<std::uint32_t>::max() - output.vertices.size()) {
        return granit::result::out_of_memory;
      }
      const auto vertex_base = static_cast<std::uint32_t>(output.vertices.size());
      for (const auto& vertex : source->VtxBuffer) {
        output.vertices.push_back({
            .x = (vertex.pos.x - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x,
            .y = (vertex.pos.y - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y,
            .u = vertex.uv.x,
            .v = vertex.uv.y,
            .color = premultiply_color(vertex.col),
        });
      }
      for (const auto& command : source->CmdBuffer) {
        if (command.UserCallback == ImDrawCallback_ResetRenderState)
          continue;
        if (command.UserCallback != nullptr)
          return granit::result::unsupported;
        if (command.ElemCount == 0)
          continue;
        const auto clip_min_x = std::clamp((command.ClipRect.x - draw_data->DisplayPos.x) *
                                               draw_data->FramebufferScale.x,
                                           0.0F, framebuffer_width);
        const auto clip_min_y = std::clamp((command.ClipRect.y - draw_data->DisplayPos.y) *
                                               draw_data->FramebufferScale.y,
                                           0.0F, framebuffer_height);
        const auto clip_max_x = std::clamp((command.ClipRect.z - draw_data->DisplayPos.x) *
                                               draw_data->FramebufferScale.x,
                                           0.0F, framebuffer_width);
        const auto clip_max_y = std::clamp((command.ClipRect.w - draw_data->DisplayPos.y) *
                                               draw_data->FramebufferScale.y,
                                           0.0F, framebuffer_height);
        if (clip_max_x <= clip_min_x || clip_max_y <= clip_min_y)
          continue;
        granit_canvas_draw_state state{};
        state.scissor = {.x = static_cast<std::int32_t>(clip_min_x),
                         .y = static_cast<std::int32_t>(clip_min_y),
                         .width = static_cast<std::uint32_t>(clip_max_x - clip_min_x),
                         .height = static_cast<std::uint32_t>(clip_max_y - clip_min_y)};
        const auto resolve_result = resolver(command.GetTexID(), state, user_data);
        if (resolve_result.failed())
          return resolve_result;
        if (command.ElemCount > std::numeric_limits<std::uint32_t>::max() - output.indices.size())
          return granit::result::out_of_memory;
        const auto first_index = static_cast<std::uint32_t>(output.indices.size());
        for (std::uint32_t index = 0; index < command.ElemCount; ++index) {
          const auto source_index = static_cast<std::size_t>(command.IdxOffset) + index;
          if (source_index >= static_cast<std::size_t>(source->IdxBuffer.Size))
            return granit::result::invalid_argument;
          const auto local_vertex =
              static_cast<std::uint64_t>(source->IdxBuffer[static_cast<int>(source_index)]) +
              command.VtxOffset;
          if (local_vertex >= static_cast<std::uint64_t>(source->VtxBuffer.Size) ||
              local_vertex > std::numeric_limits<std::uint32_t>::max() - vertex_base) {
            return granit::result::invalid_argument;
          }
          output.indices.push_back(vertex_base + static_cast<std::uint32_t>(local_vertex));
        }
        output.ranges.push_back({first_index, command.ElemCount, state});
      }
    }
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    output.clear();
    return granit::result::out_of_memory;
  } catch (...) {
    output.clear();
    return granit::result::internal;
  }
}

} // namespace granit::example::model_viewer
