// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/debug_draw_list.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstring>
#include <limits>

namespace {
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable || value == granit::result::not_ready;
}

granit_matrix4 identity() { return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; }
} // namespace

TEST_CASE("公共Debug Draw List支持批量命令、复用和句柄失效") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-debug-draw-list"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit_debug_draw_list_desc desc = GRANIT_DEBUG_DRAW_LIST_DESC_INIT;
  desc.initial_line_capacity = 4;
  desc.initial_triangle_capacity = 2;
  granit::debug_draw_list list;
  REQUIRE(list.initialize(renderer.native_handle(), desc) == granit::result::success);

  const std::array lines{granit_debug_draw_line{{0, 0, 0, UINT32_MAX},
                                                {1, 1, 1, UINT32_MAX},
                                                1,
                                                GRANIT_DEBUG_DRAW_SPACE_WORLD,
                                                GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST,
                                                0},
                         granit_debug_draw_line{{2, 3, 0, UINT32_MAX},
                                                {4, 5, 0, UINT32_MAX},
                                                2,
                                                GRANIT_DEBUG_DRAW_SPACE_SCREEN,
                                                GRANIT_DEBUG_DRAW_DEPTH_MODE_DISABLED,
                                                0}};
  const std::array triangles{
      granit_debug_draw_triangle{
          {{0, 0, 0, UINT32_MAX}, {1, 0, 0, UINT32_MAX}, {0, 1, 0, UINT32_MAX}},
          GRANIT_DEBUG_DRAW_SPACE_WORLD,
          GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST,
          {0, 0}},
      granit_debug_draw_triangle{
          {{4, 4, 0, UINT32_MAX}, {8, 4, 0, UINT32_MAX}, {4, 8, 0, UINT32_MAX}},
          GRANIT_DEBUG_DRAW_SPACE_SCREEN,
          GRANIT_DEBUG_DRAW_DEPTH_MODE_DISABLED,
          {0, 0}}};
  REQUIRE(list.append_lines(lines) == granit::result::success);
  REQUIRE(list.append_triangles(triangles) == granit::result::success);
  granit_debug_draw_list_stats stats = GRANIT_DEBUG_DRAW_LIST_STATS_INIT;
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.line_count == 2);
  CHECK(stats.triangle_count == 2);

  granit_canvas_draw_list_desc canvas_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  granit::canvas_draw_list canvas;
  REQUIRE(canvas.initialize(renderer.native_handle(), canvas_desc) == granit::result::success);
  REQUIRE(list.append_screen_to_canvas(canvas.native_handle()) == granit::result::success);
  granit_canvas_draw_list_stats canvas_stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  REQUIRE(canvas.get_stats(canvas_stats) == granit::result::success);
  CHECK(canvas_stats.vertex_count == 7);
  CHECK(canvas_stats.index_count == 9);
  CHECK(canvas_stats.item_count == 1);
  REQUIRE(list.clear() == granit::result::success);
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.line_count == 0);
  CHECK(stats.triangle_count == 0);

  auto invalid = lines[0];
  invalid.width = 0;
  CHECK(list.append_lines(std::span{&invalid, 1}) == granit::result::invalid_argument);
  invalid = lines[0];
  invalid.start.x = std::numeric_limits<float>::infinity();
  CHECK(list.append_lines(std::span{&invalid, 1}) == granit::result::invalid_argument);
  invalid = lines[0];
  invalid.end = invalid.start;
  CHECK(list.append_lines(std::span{&invalid, 1}) == granit::result::invalid_argument);
  invalid = lines[0];
  invalid.space = GRANIT_DEBUG_DRAW_SPACE_SCREEN;
  CHECK(list.append_lines(std::span{&invalid, 1}) == granit::result::invalid_argument);

  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-debug-draw-second"}) ==
          granit::result::success);
  granit_debug_draw_list_stats cross_stats = GRANIT_DEBUG_DRAW_LIST_STATS_INIT;
  CHECK(granit_debug_draw_list_get_stats(second.native_handle(), list.native_handle(),
                                         &cross_stats) == GRANIT_ERROR_INVALID_HANDLE);
  const auto old = list.native_handle();
  REQUIRE(list.destroy() == granit::result::success);
  CHECK(granit_debug_draw_list_destroy(renderer.native_handle(), old) ==
        GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("世界Debug Draw可录制到颜色附件") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-debug-world"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  const auto native = renderer.native_handle();
  constexpr uint32_t size = 16;
  granit::texture color;
  granit::texture_view color_view;
  REQUIRE(color.initialize(native, {.format = granit::texture_format::rgba8_unorm,
                                    .usage = granit::texture_usage::color_attachment |
                                             granit::texture_usage::transfer_source,
                                    .width = size,
                                    .height = size}) == granit::result::success);
  REQUIRE(color_view.initialize(native, color.native_handle()) == granit::result::success);
  granit_debug_draw_list_desc list_desc = GRANIT_DEBUG_DRAW_LIST_DESC_INIT;
  granit::debug_draw_list list;
  REQUIRE(list.initialize(native, list_desc) == granit::result::success);
  const std::array triangles{granit_debug_draw_triangle{{{-0.8F, -0.8F, 0.5F, UINT32_C(0xff0000ff)},
                                                         {0.8F, -0.8F, 0.5F, UINT32_C(0xff0000ff)},
                                                         {0, 0.8F, 0.5F, UINT32_C(0xff0000ff)}},
                                                        GRANIT_DEBUG_DRAW_SPACE_WORLD,
                                                        GRANIT_DEBUG_DRAW_DEPTH_MODE_DISABLED,
                                                        {0, 0}}};
  REQUIRE(list.append_triangles(triangles) == granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(native) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  granit_debug_draw_record_desc record = GRANIT_DEBUG_DRAW_RECORD_DESC_INIT;
  record.color = color_view.native_handle();
  record.color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  record.width = size;
  record.height = size;
  record.view_projection = identity();
  record.color_load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR;
  REQUIRE(list.record_world(recorder.native_handle(), record) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  granit::buffer readback;
  REQUIRE(readback.initialize(native, {.size = size * size * 4,
                                       .usage = granit::buffer_usage::transfer_destination,
                                       .location = granit::memory_location::readback}) ==
          granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit_texture_write_region region{0,    0,    1, GRANIT_TEXTURE_ASPECT_COLOR_BIT, 0, 0, 0,
                                           size, size, 1};
  REQUIRE(recorder.copy_texture_to_buffer(color.native_handle(), readback.native_handle(), {},
                                          region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  void* mapped = nullptr;
  REQUIRE(readback.map(0, size * size * 4, &mapped) == granit::result::success);
  std::array<uint8_t, 4> center{};
  std::memcpy(center.data(), static_cast<const std::byte*>(mapped) + (8 * size + 8) * 4, 4);
  CHECK(center == std::array<uint8_t, 4>{255, 0, 0, 255});
  REQUIRE(readback.unmap() == granit::result::success);

  auto depth_triangle = triangles[0];
  depth_triangle.depth_mode = GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST;
  REQUIRE(list.clear() == granit::result::success);
  REQUIRE(list.append_triangles(std::span{&depth_triangle, 1}) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  CHECK(list.record_world(recorder.native_handle(), record) == granit::result::invalid_argument);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  granit::texture depth;
  granit::texture_view depth_view;
  REQUIRE(depth.initialize(native, {.format = granit::texture_format::d32_float,
                                    .usage = granit::texture_usage::depth_stencil_attachment,
                                    .width = size,
                                    .height = size}) == granit::result::success);
  REQUIRE(depth_view.initialize(native, depth.native_handle()) == granit::result::success);
  granit_depth_stencil_attachment_desc depth_attachment = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth_attachment.view = depth_view.native_handle();
  depth_attachment.clear_value.depth = 0;
  granit_rendering_desc depth_clear = GRANIT_RENDERING_DESC_INIT;
  depth_clear.depth_stencil_attachment = &depth_attachment;
  depth_clear.area = {0, 0, size, size};
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(granit_command_recorder_begin_rendering(native, recorder.native_handle(), &depth_clear) ==
          GRANIT_SUCCESS);
  REQUIRE(recorder.end_rendering() == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  record.depth = depth_view.native_handle();
  record.depth_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT;
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(list.record_world(recorder.native_handle(), record) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.copy_texture_to_buffer(color.native_handle(), readback.native_handle(), {},
                                          region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  REQUIRE(readback.map(0, size * size * 4, &mapped) == granit::result::success);
  std::memcpy(center.data(), static_cast<const std::byte*>(mapped) + (8 * size + 8) * 4, 4);
  CHECK(center == std::array<uint8_t, 4>{0, 0, 0, 0});
  REQUIRE(readback.unmap() == granit::result::success);
}
