// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstring>

TEST_CASE("Canvas Pass按Batch录制顶点色与Scissor") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-ui-pass"});
  if (initialized == granit::result::backend_unavailable ||
      initialized == granit::result::incompatible_driver ||
      initialized == granit::result::no_suitable_device) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialized == granit::result::success);
  const auto native = renderer.native_handle();
  constexpr std::uint32_t size = 32;
  granit::texture color;
  granit::texture_view color_view;
  REQUIRE(color.initialize(native, {.format = granit::texture_format::rgba8_unorm,
                                    .usage = granit::texture_usage::color_attachment |
                                             granit::texture_usage::transfer_source,
                                    .width = size,
                                    .height = size}) == granit::result::success);
  REQUIRE(color_view.initialize(native, color.native_handle()) == granit::result::success);

  constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
  constexpr std::array blue{granit_canvas_vertex{3, 29, 0, 0, UINT32_MAX},
                            granit_canvas_vertex{29, 29, 1, 0, UINT32_MAX},
                            granit_canvas_vertex{16, 3, 0.5F, 1, UINT32_MAX}};
  constexpr auto red = blue;
  granit::texture blue_texture;
  granit::texture red_texture;
  granit::texture_view blue_view;
  granit::texture_view red_view;
  const granit::texture_desc sampled_desc{.format = granit::texture_format::rgba8_unorm,
                                          .usage = granit::texture_usage::sampled |
                                                   granit::texture_usage::transfer_destination,
                                          .width = 1,
                                          .height = 1};
  REQUIRE(blue_texture.initialize(native, sampled_desc) == granit::result::success);
  REQUIRE(red_texture.initialize(native, sampled_desc) == granit::result::success);
  constexpr std::array<std::uint8_t, 4> blue_pixel{0, 0, 128, 128};
  constexpr std::array<std::uint8_t, 4> red_pixel{128, 0, 0, 128};
  const granit::texture_write_region pixel_region{};
  REQUIRE(blue_texture.write(std::as_bytes(std::span{blue_pixel}), {}, pixel_region) ==
          granit::result::success);
  REQUIRE(red_texture.write(std::as_bytes(std::span{red_pixel}), {}, pixel_region) ==
          granit::result::success);
  REQUIRE(blue_view.initialize(native, blue_texture.native_handle()) == granit::result::success);
  REQUIRE(red_view.initialize(native, red_texture.native_handle()) == granit::result::success);
  granit::sampler sampler;
  REQUIRE(sampler.initialize(native, {.mag_filter = granit::filter::nearest,
                                      .min_filter = granit::filter::nearest}) ==
          granit::result::success);
  granit_canvas_draw_list_desc list_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  granit::canvas_draw_list list;
  REQUIRE(list.initialize(native, list_desc) == granit::result::success);
  REQUIRE(list.append(blue, indices,
                      {.texture = blue_view.native_handle(),
                       .sampler = sampler.native_handle(),
                       .scissor = {0, 0, size, size}}) == granit::result::success);
  REQUIRE(list.append(red, indices,
                      {.texture = red_view.native_handle(),
                       .sampler = sampler.native_handle(),
                       .scissor = {0, 0, 18, size}}) == granit::result::success);

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(native) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  granit_canvas_record_desc record_desc = GRANIT_CANVAS_RECORD_DESC_INIT;
  record_desc.color = color_view.native_handle();
  record_desc.color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  record_desc.width = size;
  record_desc.height = size;
  record_desc.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR;
  REQUIRE(list.record(recorder.native_handle(), record_desc) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  granit::buffer readback;
  REQUIRE(readback.initialize(native, {.size = size * size * 4,
                                       .usage = granit::buffer_usage::transfer_destination,
                                       .location = granit::memory_location::readback}) ==
          granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit_texture_write_region region{.mip_level = 0,
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
                                          region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  void* mapped = nullptr;
  REQUIRE(readback.map(0, size * size * 4, &mapped) == granit::result::success);
  const auto pixel = [&](std::uint32_t x) {
    std::array<std::uint8_t, 4> result{};
    std::memcpy(result.data(), static_cast<const std::byte*>(mapped) + (16 * size + x) * 4, 4);
    return result;
  };
  CHECK(pixel(14) == std::array<std::uint8_t, 4>{128, 0, 64, 192});
  CHECK(pixel(20) == std::array<std::uint8_t, 4>{0, 0, 128, 128});
  REQUIRE(readback.unmap() == granit::result::success);

  record_desc.encode_srgb = 1;
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(list.record(recorder.native_handle(), record_desc) == granit::result::success);
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
  CHECK(pixel(20) == std::array<std::uint8_t, 4>{0, 0, 188, 128});
  REQUIRE(readback.unmap() == granit::result::success);
}
