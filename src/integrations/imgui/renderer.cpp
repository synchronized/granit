// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/integrations/imgui/renderer.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

namespace granit::integration::imgui {

result append_draw_data(const ImDrawData* draw_data, canvas_draw_list& canvas,
                        texture_resolver resolver, void* user_data) noexcept {
  if (draw_data == nullptr || !canvas.valid() || resolver == nullptr)
    return result::invalid_argument;
  if (!draw_data->Valid || draw_data->CmdListsCount == 0)
    return result::success;
  const auto framebuffer_width = draw_data->DisplaySize.x * draw_data->FramebufferScale.x;
  const auto framebuffer_height = draw_data->DisplaySize.y * draw_data->FramebufferScale.y;
  if (framebuffer_width <= 0.0F || framebuffer_height <= 0.0F ||
      framebuffer_width > static_cast<float>(std::numeric_limits<std::int32_t>::max()) ||
      framebuffer_height > static_cast<float>(std::numeric_limits<std::int32_t>::max()))
    return result::invalid_argument;
  try {
    std::vector<granit_canvas_vertex> vertices;
    std::vector<std::uint32_t> indices;
    for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
      const auto* source = draw_data->CmdLists[list_index];
      vertices.resize(static_cast<std::size_t>(source->VtxBuffer.Size));
      for (int vertex_index = 0; vertex_index < source->VtxBuffer.Size; ++vertex_index) {
        const auto& vertex = source->VtxBuffer[vertex_index];
        vertices[static_cast<std::size_t>(vertex_index)] = {
            .x = (vertex.pos.x - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x,
            .y = (vertex.pos.y - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y,
            .u = vertex.uv.x,
            .v = vertex.uv.y,
            .color = vertex.col,
        };
      }
      for (const auto& command : source->CmdBuffer) {
        if (command.UserCallback == ImDrawCallback_ResetRenderState)
          continue;
        if (command.UserCallback != nullptr)
          return result::unsupported;
        indices.resize(command.ElemCount);
        for (std::uint32_t index = 0; index < command.ElemCount; ++index) {
          const auto source_index = static_cast<std::size_t>(command.IdxOffset) + index;
          const auto value = static_cast<std::uint64_t>(source->IdxBuffer[source_index]) +
                             static_cast<std::uint64_t>(command.VtxOffset);
          if (value >= vertices.size())
            return result::invalid_argument;
          indices[index] = static_cast<std::uint32_t>(value);
        }
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
        if (resolve_result != result::success)
          return resolve_result;
        const auto append_result = canvas.append(vertices, indices, state);
        if (append_result != result::success)
          return append_result;
      }
    }
    return result::success;
  } catch (const std::bad_alloc&) {
    return result::out_of_memory;
  } catch (...) {
    return result::internal;
  }
}

} // namespace granit::integration::imgui
