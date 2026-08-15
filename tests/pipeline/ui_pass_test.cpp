// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/ui_pass.h"

#include <granit/granit.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

granit::math::matrix4 identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }

std::vector<char> load_package() {
  std::ifstream stream{GRANIT_UNLIT_UI_TEST_PACKAGE, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

} // namespace

TEST_CASE("UI Pass按Batch录制顶点色与Scissor") {
  using namespace granit::pipeline::detail;
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
  constexpr std::array blue{canvas_vertex{-0.8F, -0.8F, 0, 0, UINT32_MAX},
                            canvas_vertex{0.8F, -0.8F, 1, 0, UINT32_MAX},
                            canvas_vertex{0.0F, 0.8F, 0.5F, 1, UINT32_MAX}};
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
  canvas_draw_list list;
  REQUIRE(list.append(blue, indices,
                      {.texture = blue_view.native_handle(),
                       .sampler = sampler.native_handle(),
                       .scissor = {0, 0, size, size}}) == GRANIT_SUCCESS);
  REQUIRE(list.append(red, indices,
                      {.texture = red_view.native_handle(),
                       .sampler = sampler.native_handle(),
                       .scissor = {0, 0, 18, size}}) == GRANIT_SUCCESS);
  ui_geometry_upload geometry;
  REQUIRE(geometry.upload(native, list) == GRANIT_SUCCESS);

  const auto archive = load_package();
  REQUIRE_FALSE(archive.empty());
  const std::array<float, 4> white{1, 1, 1, 1};
  const std::array updates{
      granit_material_parameter_update{granit_material_parameter_id("base_color", 10),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, white.data(),
                                       sizeof(white), GRANIT_NULL_HANDLE},
      granit_material_parameter_update{granit_material_parameter_id("base_color_texture", 18),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       blue_view.native_handle()},
      granit_material_parameter_update{granit_material_parameter_id("unlit_sampler", 13),
                                       GRANIT_MATERIAL_PARAMETER_SAMPLER, 0, nullptr, 0,
                                       sampler.native_handle()}};
  granit_material_desc material_desc = GRANIT_MATERIAL_DESC_INIT;
  material_desc.archive_data = archive.data();
  material_desc.archive_size = archive.size();
  material_desc.initial_updates = updates.data();
  material_desc.initial_update_count = static_cast<std::uint32_t>(updates.size());
  granit_material material = GRANIT_NULL_HANDLE;
  REQUIRE(granit_material_create(native, &material_desc, &material) == GRANIT_SUCCESS);

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(native) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit::material::pbr_frame_constants frame{.view_projection = identity(),
                                                    .camera_position = {},
                                                    .direction_to_light = {},
                                                    .light_radiance = {}};
  const granit::material::pbr_object_constants object{
      .model = identity(), .normal_matrix = identity(), .object_id = {}};
  REQUIRE(record_ui_pass(native, recorder.native_handle(),
                         {.color = color_view.native_handle(),
                          .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                          .width = size,
                          .height = size,
                          .material = material,
                          .frame = frame,
                          .object = object,
                          .load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR},
                         list, geometry) == GRANIT_SUCCESS);
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
  REQUIRE(granit_material_destroy(native, material) == GRANIT_SUCCESS);
}
