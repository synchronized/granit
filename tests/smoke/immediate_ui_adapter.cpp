// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

struct foreign_vertex {
  float position[2];
  float uv[2];
  std::uint32_t color;
};

struct foreign_draw_command {
  std::uint32_t first_index;
  std::uint32_t index_count;
  granit_texture_view texture;
  granit_sampler sampler;
  granit_scissor scissor;
};

granit::result append_foreign_draw_data(
    granit::canvas_draw_list& canvas, std::span<const foreign_vertex> source_vertices,
    std::span<const std::uint32_t> source_indices,
    std::span<const foreign_draw_command> commands) {
  std::vector<granit_canvas_vertex> vertices;
  vertices.reserve(source_vertices.size());
  for (const auto& vertex : source_vertices) {
    // 显式逐字段转换，不依赖第三方顶点结构与 Granit ABI 恰好具有相同布局。
    vertices.push_back({vertex.position[0], vertex.position[1], vertex.uv[0], vertex.uv[1],
                        vertex.color});
  }
  for (const auto& command : commands) {
    if (command.first_index > source_indices.size() ||
        command.index_count > source_indices.size() - command.first_index) {
      return granit::result::invalid_argument;
    }
    const auto indices = source_indices.subspan(command.first_index, command.index_count);
    const granit_canvas_draw_state state{command.texture, command.sampler, command.scissor};
    const auto appended = canvas.append(vertices, indices, state);
    if (appended != granit::result::success)
      return appended;
  }
  return granit::result::success;
}

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

int main() {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-ui-adapter"});
  if (environment_unavailable(initialized)) {
    std::cout << "当前环境没有可用 Vulkan 设备，跳过 Adapter 运行验证。\n";
    return 0;
  }
  if (initialized != granit::result::success)
    return 1;
  const auto native = renderer.native_handle();

  granit::texture texture;
  granit::texture_view view;
  granit::sampler sampler;
  if (texture.initialize(native, {.format = granit::texture_format::rgba8_unorm,
                                  .usage = granit::texture_usage::sampled,
                                  .width = 1,
                                  .height = 1}) != granit::result::success ||
      view.initialize(native, texture.native_handle()) != granit::result::success ||
      sampler.initialize(native, {}) != granit::result::success) {
    return 1;
  }

  constexpr std::array vertices{
      foreign_vertex{{0, 0}, {0, 0}, UINT32_C(0xffffffff)},
      foreign_vertex{{64, 0}, {1, 0}, UINT32_C(0xffffffff)},
      foreign_vertex{{64, 32}, {1, 1}, UINT32_C(0xffffffff)},
      foreign_vertex{{0, 32}, {0, 1}, UINT32_C(0xffffffff)}};
  constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 2, 3, 0};
  const std::array commands{foreign_draw_command{0, static_cast<std::uint32_t>(indices.size()),
                                                  view.native_handle(), sampler.native_handle(),
                                                  {0, 0, 64, 32}}};

  granit_canvas_draw_list_desc desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  granit::canvas_draw_list canvas;
  if (canvas.initialize(native, desc) != granit::result::success ||
      append_foreign_draw_data(canvas, vertices, indices, commands) != granit::result::success) {
    return 1;
  }
  granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  if (canvas.get_stats(stats) != granit::result::success || stats.vertex_count != 4 ||
      stats.index_count != 6 || stats.batch_count != 1) {
    return 1;
  }
  std::cout << "立即式 UI Adapter 已生成 4 个顶点、6 个索引和 1 个 Canvas Batch。\n";
  return 0;
}
