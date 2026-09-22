// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/text_atlas.hpp>
#include <granit/pipeline/text_draw_list.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstring>
#include <limits>

namespace {
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable || value == granit::result::not_ready;
}

TEST_CASE("Text Draw List通过R8 Atlas批量生成Canvas四边形") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-text-canvas"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::text_atlas atlas;
  REQUIRE(atlas.initialize(renderer,
                           {.page_width = 8, .page_height = 8, .max_pages = 8, .padding = 1}) ==
          granit::result::success);
  constexpr std::array<uint8_t, 4> bitmap{255, 128, 64, 0};
  const granit::text_glyph_bitmap_desc bitmap_desc{.glyph_id = 11,
                                                   .font_key = 7,
                                                   .width = 2,
                                                   .height = 2,
                                                   .bearing_x = 1,
                                                   .bearing_y = 2,
                                                   .bitmap = bitmap,
                                                   .bytes_per_row = 0};
  REQUIRE(atlas.upload_glyph(bitmap_desc) == granit::result::success);

  granit::text_draw_list text;
  REQUIRE(text.initialize(renderer) == granit::result::success);
  const std::array glyphs{
      granit::text_glyph_instance{
          .font_key = 7, .glyph_id = 11, .color = UINT32_C(0xff0000ff), .x = 3, .y = 5},
      granit::text_glyph_instance{
          .font_key = 7, .glyph_id = 11, .color = UINT32_C(0xff00ff00), .x = 6, .y = 5}};
  REQUIRE(text.append_glyph_run(glyphs, {0, 0, 16, 16}) == granit::result::success);

  granit::canvas_draw_list canvas;
  REQUIRE(canvas.initialize(renderer) == granit::result::success);
  REQUIRE(text.append_to_canvas(atlas, canvas.ref()) == granit::result::success);
  granit::canvas_draw_list_stats stats;
  REQUIRE(canvas.get_stats(stats) == granit::result::success);
  CHECK(stats.vertex_count == 8);
  CHECK(stats.index_count == 12);
  CHECK(stats.item_count == 2);
  CHECK(stats.batch_count == 1);

  REQUIRE(text.clear() == granit::result::success);
  const granit::text_glyph_instance missing{
      .font_key = 7, .glyph_id = 99, .color = UINT32_MAX, .x = 0, .y = 0};
  REQUIRE(text.append_glyph_run(std::span{&missing, 1}) == granit::result::success);
  CHECK(text.append_to_canvas(atlas, canvas.ref()) == granit::result::not_ready);
}

TEST_CASE("Text Atlas覆盖率进入像素且跨页保持Draw顺序") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-text-pixel"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  const auto native = renderer.native_handle();

  granit::text_atlas atlas;
  REQUIRE(atlas.initialize(renderer,
                           {.page_width = 8, .page_height = 8, .max_pages = 2, .padding = 1}) ==
          granit::result::success);
  constexpr std::array<uint8_t, 8> coverage{128, 128, 128, 128, 128, 128, 128, 128};
  for (const uint32_t glyph_id : {1U, 2U, 3U}) {
    const granit::text_glyph_bitmap_desc glyph{.glyph_id = glyph_id,
                                               .font_key = 9,
                                               .width = 4,
                                               .height = 2,
                                               .bearing_x = 0,
                                               .bearing_y = 2,
                                               .bitmap = coverage,
                                               .bytes_per_row = 0};
    REQUIRE(atlas.upload_glyph(glyph) == granit::result::success);
  }
  granit::text_atlas_stats atlas_stats;
  REQUIRE(atlas.get_stats(atlas_stats) == granit::result::success);
  REQUIRE(atlas_stats.page_count == 2);

  granit::text_draw_list text;
  REQUIRE(text.initialize(renderer) == granit::result::success);
  // 第 1、2 个元素来自不同页面，第 3 个回到第一页，不能跨中间 Draw 重排合批。
  const std::array glyphs{
      granit::text_glyph_instance{
          .font_key = 9, .glyph_id = 1, .color = UINT32_C(0xff0000ff), .x = 4, .y = 6},
      granit::text_glyph_instance{
          .font_key = 9, .glyph_id = 3, .color = UINT32_C(0xff00ff00), .x = 12, .y = 6},
      granit::text_glyph_instance{
          .font_key = 9, .glyph_id = 2, .color = UINT32_C(0xffff0000), .x = 20, .y = 6}};
  REQUIRE(text.append_glyph_run(glyphs, {0, 0, 32, 32}) == granit::result::success);
  granit::canvas_draw_list canvas;
  REQUIRE(canvas.initialize(renderer) == granit::result::success);
  REQUIRE(text.append_to_canvas(atlas, canvas.ref()) == granit::result::success);
  granit::canvas_draw_list_stats canvas_stats;
  REQUIRE(canvas.get_stats(canvas_stats) == granit::result::success);
  CHECK(canvas_stats.item_count == 3);
  CHECK(canvas_stats.batch_count == 3);

  constexpr uint32_t size = 32;
  granit::texture color;
  granit::texture_view color_view;
  REQUIRE(color.initialize(granit::renderer_ref::from_native(native),
                           {.format = granit::texture_format::rgba8_unorm,
                            .usage = granit::texture_usage::color_attachment |
                                     granit::texture_usage::transfer_source,
                            .width = size,
                            .height = size}) == granit::result::success);
  REQUIRE(color_view.initialize(renderer, color) == granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit::canvas_record_desc record{
      .color = color_view.ref(),
      .color_format = granit::texture_format::rgba8_unorm,
      .width = size,
      .height = size,
      .load_operation = granit::attachment_load_operation::clear,
      .encode_srgb = false,
      .frame_slot = granit::canvas_frame_slot_auto,
  };
  REQUIRE(canvas.record(recorder, record) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  granit::buffer readback;
  REQUIRE(readback.initialize(granit::renderer_ref::from_native(native),
                              {.size = size * size * 4,
                               .usage = granit::buffer_usage::transfer_destination,
                               .location = granit::memory_location::readback}) ==
          granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit_texture_write_region readback_region{.mip_level = 0,
                                                    .base_array_layer = 0,
                                                    .array_layer_count = 1,
                                                    .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                    .x = 0,
                                                    .y = 0,
                                                    .z = 0,
                                                    .width = size,
                                                    .height = size,
                                                    .depth = 1};
  REQUIRE(recorder.copy_texture_to_buffer(color.native_handle(), readback.native_handle(), {},
                                          readback_region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  void* mapped = nullptr;
  REQUIRE(readback.map(0, size * size * 4, &mapped) == granit::result::success);
  std::array<uint8_t, 4> pixel{};
  // Canvas 与回读像素均使用左上原点，y=4 必须保持在第 4 行。
  std::memcpy(pixel.data(), static_cast<const uint8_t*>(mapped) + (4 * size + 5) * 4, pixel.size());
  CHECK(pixel[0] == 255);
  CHECK(pixel[1] == 0);
  CHECK(pixel[2] == 0);
  CHECK(pixel[3] == Catch::Approx(128).margin(1));
  std::memcpy(pixel.data(), static_cast<const uint8_t*>(mapped) + (27 * size + 5) * 4,
              pixel.size());
  CHECK(pixel == std::array<uint8_t, 4>{0, 0, 0, 0});
  REQUIRE(readback.unmap() == granit::result::success);
}
} // namespace

TEST_CASE("公共Text Draw List保存已整形字形并校验句柄") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-text-draw-list"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::text_draw_list list;
  REQUIRE(list.initialize(renderer, {.initial_glyph_capacity = 8, .initial_run_capacity = 2}) ==
          granit::result::success);
  const std::array glyphs{
      granit::text_glyph_instance{
          .font_key = 1, .glyph_id = 42, .color = UINT32_C(0xffffffff), .x = 10, .y = 20},
      granit::text_glyph_instance{
          .font_key = 2, .glyph_id = 0, .color = UINT32_C(0xff00ffff), .x = 18.5F, .y = 20}};
  REQUIRE(list.append_glyph_run(glyphs, {1, 2, 30, 40}) == granit::result::success);
  granit::text_draw_list_stats stats;
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.glyph_count == 2);
  CHECK(stats.run_count == 1);

  auto invalid = glyphs[0];
  invalid.font_key = 0;
  CHECK(list.append_glyph_run(std::span{&invalid, 1}) == granit::result::invalid_argument);
  invalid = glyphs[0];
  invalid.x = std::numeric_limits<float>::infinity();
  CHECK(list.append_glyph_run(std::span{&invalid, 1}) == granit::result::invalid_argument);
  CHECK(list.append_glyph_run(std::span{&glyphs[0], 1}, {1, 1, 0, 3}) ==
        granit::result::invalid_argument);
  CHECK(list.append_glyph_run(std::span<const granit::text_glyph_instance>{}) ==
        granit::result::invalid_argument);

  REQUIRE(list.clear() == granit::result::success);
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.glyph_count == 0);
  CHECK(stats.run_count == 0);

  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-text-draw-second"}) ==
          granit::result::success);
  granit_text_draw_list_stats cross_stats = GRANIT_TEXT_DRAW_LIST_STATS_INIT;
  CHECK(granit_text_draw_list_get_stats(second.native_handle(), list.native_handle(),
                                        &cross_stats) == GRANIT_ERROR_INVALID_HANDLE);
  const auto old = list.native_handle();
  REQUIRE(list.destroy() == granit::result::success);
  CHECK(granit_text_draw_list_destroy(renderer.native_handle(), old) ==
        GRANIT_ERROR_INVALID_HANDLE);
}
