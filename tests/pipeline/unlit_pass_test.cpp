// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/unlit_pass.h"

#include <granit/granit.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

granit::math::matrix4 identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }

std::vector<char> load_package() {
  std::ifstream stream{GRANIT_UNLIT_TEST_PACKAGE, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

} // namespace

TEST_CASE("Unlit Opaque与Alpha Cutoff产生预期像素") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-unlit-pass"});
  if (initialized == granit::result::backend_unavailable ||
      initialized == granit::result::incompatible_driver ||
      initialized == granit::result::no_suitable_device) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialized == granit::result::success);
  const auto native = renderer.native_handle();
  constexpr uint32_t size = 32;
  granit::texture color;
  granit::texture_view color_view;
  granit::texture depth;
  granit::texture_view depth_view;
  REQUIRE(color.initialize(native, {.format = granit::texture_format::rgba8_unorm,
                                    .usage = granit::texture_usage::color_attachment |
                                             granit::texture_usage::transfer_source,
                                    .width = size,
                                    .height = size}) == granit::result::success);
  REQUIRE(color_view.initialize(native, color.native_handle()) == granit::result::success);
  REQUIRE(depth.initialize(native, {.format = granit::texture_format::d32_float,
                                    .usage = granit::texture_usage::depth_stencil_attachment,
                                    .width = size,
                                    .height = size}) == granit::result::success);
  REQUIRE(depth_view.initialize(native, depth.native_handle()) == granit::result::success);

  constexpr std::array<float, 9> positions{-0.8F, -0.8F, 0.5F, 0.8F, -0.8F, 0.5F, 0.0F, 0.8F, 0.5F};
  granit::buffer vertices;
  REQUIRE(vertices.initialize(native,
                              {.size = sizeof(positions),
                               .usage = granit::buffer_usage::vertex,
                               .location = granit::memory_location::device},
                              std::as_bytes(std::span{positions})) == granit::result::success);
  const granit_vertex_attribute attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0, 0};
  const granit_mesh_vertex_buffer vertex{
      vertices.native_handle(), 0, {12, GRANIT_VERTEX_STEP_MODE_VERTEX, 1, 0, &attribute}};
  granit_mesh_desc mesh_desc = GRANIT_MESH_DESC_INIT;
  mesh_desc.vertex_buffers = &vertex;
  mesh_desc.vertex_buffer_count = 1;
  mesh_desc.vertex_count = 3;
  granit_mesh mesh = GRANIT_NULL_HANDLE;
  REQUIRE(granit_mesh_create(native, &mesh_desc, &mesh) == GRANIT_SUCCESS);

  const auto archive = load_package();
  REQUIRE_FALSE(archive.empty());
  granit::buffer readback;
  REQUIRE(readback.initialize(native, {.size = size * size * 4,
                                       .usage = granit::buffer_usage::transfer_destination,
                                       .location = granit::memory_location::readback}) ==
          granit::result::success);
  const auto read_pixel = [&](uint32_t x, uint32_t y) {
    granit::command_recorder recorder;
    REQUIRE(recorder.initialize(native) == granit::result::success);
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
    const auto* pixel = static_cast<const uint8_t*>(mapped) + (y * size + x) * 4;
    const std::array result{pixel[0], pixel[1], pixel[2], pixel[3]};
    REQUIRE(readback.unmap() == granit::result::success);
    return result;
  };
  const auto render =
      [&](std::array<float, 4> base_color, granit::pipeline::detail::unlit_mode mode,
          granit_attachment_load_operation load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR,
          granit_scissor scissor = {}) {
        const float alpha_cutoff = 0.5F;
        const std::array updates{
            granit_material_parameter_update{granit_material_parameter_id("base_color", 10),
                                             GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, base_color.data(),
                                             sizeof(base_color), GRANIT_NULL_HANDLE},
            granit_material_parameter_update{granit_material_parameter_id("alpha_cutoff", 12),
                                             GRANIT_MATERIAL_PARAMETER_FLOAT32, 0, &alpha_cutoff,
                                             sizeof(alpha_cutoff), GRANIT_NULL_HANDLE}};
        granit_material_desc material_desc = GRANIT_MATERIAL_DESC_INIT;
        material_desc.archive_data = archive.data();
        material_desc.archive_size = archive.size();
        material_desc.initial_updates = updates.data();
        material_desc.initial_update_count = static_cast<uint32_t>(updates.size());
        granit_material material = GRANIT_NULL_HANDLE;
        REQUIRE(granit_material_create(native, &material_desc, &material) == GRANIT_SUCCESS);
        granit::command_recorder recorder;
        REQUIRE(recorder.initialize(native) == granit::result::success);
        REQUIRE(recorder.begin() == granit::result::success);
        granit::material::pbr_frame_constants frame{.view_projection = identity(),
                                                    .camera_position = {},
                                                    .direction_to_light = {},
                                                    .light_radiance = {}};
        granit::material::pbr_object_constants object{
            .model = identity(), .normal_matrix = identity(), .object_id = {}};
        REQUIRE(granit::pipeline::detail::record_unlit_pass(
                    native, recorder.native_handle(),
                    {.color = color_view.native_handle(),
                     .depth = depth_view.native_handle(),
                     .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                     .depth_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT,
                     .width = size,
                     .height = size,
                     .mesh = mesh,
                     .material = material,
                     .frame = frame,
                     .object = object,
                     .mode = mode,
                     .color_load_operation = load_operation,
                     .scissor = scissor}) == GRANIT_SUCCESS);
        REQUIRE(recorder.end() == granit::result::success);
        REQUIRE(recorder.submit() == granit::result::success);
        REQUIRE(recorder.reset() == granit::result::success);
        REQUIRE(granit_material_destroy(native, material) == GRANIT_SUCCESS);
      };

  render({0.25F, 0.5F, 1.0F, 1.0F}, granit::pipeline::detail::unlit_mode::opaque);
  CHECK(read_pixel(size / 2, size / 2) == std::array<uint8_t, 4>{64, 128, 255, 255});
  render({0.25F, 0.5F, 1.0F, 0.25F}, granit::pipeline::detail::unlit_mode::alpha_cutoff);
  const auto cutoff_pixel = read_pixel(size / 2, size / 2);
  CHECK(cutoff_pixel[0] == 0);
  CHECK(cutoff_pixel[1] == 0);
  CHECK(cutoff_pixel[2] == 0);

  render({0.0F, 0.0F, 0.5F, 0.5F}, granit::pipeline::detail::unlit_mode::transparent);
  render({0.5F, 0.0F, 0.0F, 0.5F}, granit::pipeline::detail::unlit_mode::transparent,
         GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD, {0, 0, 18, size});
  CHECK(read_pixel(14, size / 2) == std::array<uint8_t, 4>{128, 0, 64, 191});
  CHECK(read_pixel(20, size / 2) == std::array<uint8_t, 4>{0, 0, 128, 128});
  REQUIRE(granit_mesh_destroy(native, mesh) == GRANIT_SUCCESS);
}
