// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include <array>
#include <string_view>

#include "backend/webgpu/renderer_state.h"

TEST_CASE("WebGPU Renderer 状态集中管理静态 Provider 生命周期", "[backend][webgpu][renderer]") {
  granit::detail::backend_plugin_loader module;
  REQUIRE(module.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);

  granit::detail::webgpu_renderer_state state;
  granit::detail::backend_renderer& backend = state;
  constexpr auto surface_types = GRANIT_SURFACE_TYPE_WIN32_BIT | GRANIT_SURFACE_TYPE_XCB_BIT |
                                 GRANIT_SURFACE_TYPE_WAYLAND_BIT | GRANIT_SURFACE_TYPE_CANVAS_BIT;
  REQUIRE(state.initialize_static(module.api(), surface_types, nullptr, nullptr) == GRANIT_SUCCESS);
  for (int attempt = 0; attempt < 2 && backend.lifecycle_status().state ==
                                           granit::detail::backend_lifecycle_state::initializing;
       ++attempt) {
    REQUIRE(backend.process_backend_events() == GRANIT_SUCCESS);
  }

  REQUIRE(backend.lifecycle_status().state == granit::detail::backend_lifecycle_state::ready);
  CHECK(backend.lifecycle_status().failure_result == GRANIT_SUCCESS);
  CHECK(backend.capabilities().uniform_buffer_offset_alignment == 256);
  CHECK(backend.capabilities().storage_buffer_offset_alignment == 256);
  CHECK(backend.capabilities().max_uniform_buffer_binding_size == 65536);
  CHECK(state.presentation() != nullptr);

  auto vertex_buffer = state.allocate_buffer_resource();
  REQUIRE(vertex_buffer != nullptr);
  granit_buffer_desc vertex_desc = GRANIT_BUFFER_DESC_INIT;
  vertex_desc.size = 64;
  vertex_desc.usage = GRANIT_BUFFER_USAGE_VERTEX_BIT | GRANIT_BUFFER_USAGE_INDEX_BIT |
                      GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  vertex_desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
  REQUIRE(state.create_buffer(vertex_desc, *vertex_buffer) == GRANIT_SUCCESS);
  const std::uint32_t geometry[]{0, 1, 2, 3};
  REQUIRE(state.upload_buffer(*vertex_buffer, 0, geometry, sizeof(geometry)) == GRANIT_SUCCESS);

  auto upload_buffer = state.allocate_buffer_resource();
  REQUIRE(upload_buffer != nullptr);
  auto upload_desc = vertex_desc;
  upload_desc.usage = GRANIT_BUFFER_USAGE_VERTEX_BIT;
  upload_desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
  REQUIRE(state.create_buffer(upload_desc, *upload_buffer) == GRANIT_SUCCESS);
  auto* mapped = static_cast<std::uint32_t*>(state.mapped_buffer_data(*upload_buffer));
  REQUIRE(mapped != nullptr);
  mapped[0] = 42;
  REQUIRE(state.flush_buffer(*upload_buffer, 0, sizeof(std::uint32_t)) == GRANIT_SUCCESS);

  auto texture = state.allocate_texture_resource();
  REQUIRE(texture != nullptr);
  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT |
                       GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT |
                       GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.width = 64;
  texture_desc.height = 32;
  texture_desc.mip_levels = 4;
  REQUIRE(state.create_texture(texture_desc, *texture) == GRANIT_SUCCESS);
  const std::array<std::uint8_t, 32> mip_pixels{};
  const granit_texture_data_layout mip_layout{
      .offset = 0, .bytes_per_row = 16, .rows_per_image = 2};
  const granit_texture_write_region mip_region{.mip_level = 1,
                                               .base_array_layer = 0,
                                               .array_layer_count = 1,
                                               .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                               .x = 2,
                                               .y = 3,
                                               .z = 0,
                                               .width = 3,
                                               .height = 2,
                                               .depth = 1};
  REQUIRE(state.upload_texture(*texture, texture_desc.format, mip_pixels.data(), mip_pixels.size(),
                               mip_layout, mip_region) == GRANIT_SUCCESS);
  const std::array<granit::detail::backend_upload_operation, 2> mixed_uploads{{
      {.type = granit::detail::backend_upload_type::buffer,
       .buffer = vertex_buffer.get(),
       .destination_offset = 16,
       .data = geometry,
       .size = sizeof(geometry)},
      {.type = granit::detail::backend_upload_type::texture,
       .texture = texture.get(),
       .data = mip_pixels.data(),
       .size = mip_pixels.size(),
       .texture_copy = {.buffer_row_length = 4,
                        .buffer_image_height = 2,
                        .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                        .mip_level = 1,
                        .base_array_layer = 0,
                        .array_layer_count = 1,
                        .x = 2,
                        .y = 3,
                        .z = 0,
                        .width = 3,
                        .height = 2,
                        .depth = 1}},
  }};
  REQUIRE(state.upload_batch(mixed_uploads) == GRANIT_SUCCESS);
  CHECK(state.upload_batch({}) == GRANIT_ERROR_INVALID_ARGUMENT);

  auto texture_view = state.allocate_texture_view_resource();
  REQUIRE(texture_view != nullptr);
  granit_texture_view_desc view_desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
  view_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  view_desc.range.base_mip_level = 1;
  view_desc.range.mip_level_count = 2;
  REQUIRE(state.create_texture_view(*texture, texture_desc, view_desc, *texture_view) ==
          GRANIT_SUCCESS);

  auto sampler = state.allocate_sampler_resource();
  REQUIRE(sampler != nullptr);
  granit_sampler_desc sampler_desc = GRANIT_SAMPLER_DESC_INIT;
  sampler_desc.mag_filter = GRANIT_FILTER_NEAREST;
  sampler_desc.address_mode_v = GRANIT_ADDRESS_MODE_MIRRORED_REPEAT;
  sampler_desc.min_lod = 1.0F;
  sampler_desc.max_lod = 3.0F;
  REQUIRE(state.create_sampler(sampler_desc, *sampler) == GRANIT_SUCCESS);

  auto anisotropic_sampler = state.allocate_sampler_resource();
  REQUIRE(anisotropic_sampler != nullptr);
  auto anisotropic_desc = sampler_desc;
  anisotropic_desc.mag_filter = GRANIT_FILTER_LINEAR;
  anisotropic_desc.compare_operation = GRANIT_COMPARE_OPERATION_DISABLED;
  anisotropic_desc.anisotropy_enabled = 1;
  anisotropic_desc.max_anisotropy = 4.0F;
  REQUIRE(state.create_sampler(anisotropic_desc, *anisotropic_sampler) == GRANIT_SUCCESS);

  auto unsupported_sampler = state.allocate_sampler_resource();
  REQUIRE(unsupported_sampler != nullptr);
  sampler_desc.lod_bias = 0.5F;
  CHECK(state.create_sampler(sampler_desc, *unsupported_sampler) == GRANIT_ERROR_UNSUPPORTED);
  sampler_desc.lod_bias = 0.0F;
  sampler_desc.compare_operation = GRANIT_COMPARE_OPERATION_LESS_EQUAL;
  CHECK(state.create_sampler(sampler_desc, *unsupported_sampler) == GRANIT_ERROR_UNSUPPORTED);

  auto uniform_buffer = state.allocate_buffer_resource();
  REQUIRE(uniform_buffer != nullptr);
  granit_buffer_desc uniform_desc = GRANIT_BUFFER_DESC_INIT;
  uniform_desc.size = 256;
  uniform_desc.usage =
      GRANIT_BUFFER_USAGE_UNIFORM_BIT | GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  REQUIRE(state.create_buffer(uniform_desc, *uniform_buffer) == GRANIT_SUCCESS);
  const std::array<granit_bind_group_layout_entry, 3> binding_layout{{
      {7, GRANIT_BINDING_TYPE_UNIFORM_BUFFER, 1, GRANIT_SHADER_STAGE_VERTEX_BIT},
      {2, GRANIT_BINDING_TYPE_SAMPLED_TEXTURE, 1, GRANIT_SHADER_STAGE_FRAGMENT_BIT},
      {9, GRANIT_BINDING_TYPE_SAMPLER, 1, GRANIT_SHADER_STAGE_FRAGMENT_BIT},
  }};
  auto bind_group_layout = state.allocate_bind_group_layout_resource();
  REQUIRE(bind_group_layout != nullptr);
  REQUIRE(state.create_bind_group_layout(binding_layout, *bind_group_layout) == GRANIT_SUCCESS);
  const std::array<granit::detail::backend_bind_group_write, 3> writes{{
      {9, 0, granit::detail::backend_binding_type::sampler, nullptr, 0, 0, nullptr, sampler.get()},
      {7, 0, granit::detail::backend_binding_type::uniform_buffer, uniform_buffer.get(), 64, 128,
       nullptr, nullptr},
      {2, 0, granit::detail::backend_binding_type::sampled_texture, nullptr, 0, 0,
       texture_view.get(), nullptr},
  }};
  auto bind_group = state.allocate_bind_group_resource();
  REQUIRE(bind_group != nullptr);
  REQUIRE(state.create_bind_group(*bind_group_layout, writes, *bind_group) == GRANIT_SUCCESS);

  auto unsupported_layout = state.allocate_bind_group_layout_resource();
  REQUIRE(unsupported_layout != nullptr);
  auto array_layout = binding_layout;
  array_layout[0].array_count = 2;
  CHECK(state.create_bind_group_layout(array_layout, *unsupported_layout) ==
        GRANIT_ERROR_UNSUPPORTED);

  auto pipeline_layout = state.allocate_pipeline_layout_resource();
  REQUIRE(pipeline_layout != nullptr);
  const std::array<granit::detail::backend_bind_group_layout_resource*, 2> pipeline_layouts{
      bind_group_layout.get(), bind_group_layout.get()};
  REQUIRE(state.create_pipeline_layout(pipeline_layouts, *pipeline_layout) == GRANIT_SUCCESS);
  auto compute_shader = state.allocate_shader_resource();
  REQUIRE(compute_shader != nullptr);
  constexpr std::string_view compute_source = "@compute @workgroup_size(1) fn cs_main() {}";
  REQUIRE(state.create_wgsl_shader(*compute_shader, GRANIT_SHADER_STAGE_COMPUTE, compute_source,
                                   "cs_main") == GRANIT_SUCCESS);
  auto compute_pipeline = state.allocate_compute_pipeline_resource();
  REQUIRE(compute_pipeline != nullptr);
  REQUIRE(state.create_compute_pipeline(*pipeline_layout, *compute_shader, "cs_main",
                                        *compute_pipeline) == GRANIT_SUCCESS);
  auto compute_recorder = state.allocate_command_recorder_resource();
  REQUIRE(compute_recorder != nullptr);
  REQUIRE(state.create_command_recorder(*compute_recorder) == GRANIT_SUCCESS);
  REQUIRE(state.begin_command_recorder(*compute_recorder) == GRANIT_SUCCESS);
  CHECK(state.dispatch(*compute_recorder, 1, 1, 1) == GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(state.bind_compute_pipeline(*compute_recorder, *compute_pipeline) == GRANIT_SUCCESS);
  const std::array<granit::detail::backend_bind_group_resource*, 1> compute_groups{
      bind_group.get()};
  REQUIRE(state.bind_compute_groups(*compute_recorder, *pipeline_layout, 0, compute_groups, {}, {},
                                    {}) == GRANIT_SUCCESS);
  REQUIRE(state.dispatch(*compute_recorder, 2, 1, 1) == GRANIT_SUCCESS);
  const granit::detail::backend_color_attachment color_attachment{
      texture.get(),
      texture_view.get(),
      {},
      GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
      GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR,
      GRANIT_ATTACHMENT_STORE_OPERATION_STORE,
      {0.0F, 0.0F, 0.0F, 1.0F}};
  REQUIRE(state.begin_rendering(*compute_recorder, {}, std::span{&color_attachment, 1}, nullptr,
                                1) == GRANIT_SUCCESS);
  REQUIRE(state.end_rendering(*compute_recorder) == GRANIT_SUCCESS);
  REQUIRE(state.end_command_recorder(*compute_recorder) == GRANIT_SUCCESS);

  auto compute_copy_recorder = state.allocate_command_recorder_resource();
  REQUIRE(compute_copy_recorder != nullptr);
  REQUIRE(state.create_command_recorder(*compute_copy_recorder) == GRANIT_SUCCESS);
  REQUIRE(state.begin_command_recorder(*compute_copy_recorder) == GRANIT_SUCCESS);
  REQUIRE(state.bind_compute_pipeline(*compute_copy_recorder, *compute_pipeline) == GRANIT_SUCCESS);
  REQUIRE(state.dispatch(*compute_copy_recorder, 1, 1, 1) == GRANIT_SUCCESS);
  const granit_texture_data_layout copy_layout{
      .offset = 0, .bytes_per_row = 256, .rows_per_image = 1};
  const granit_texture_write_region copy_region{.mip_level = 0,
                                                .base_array_layer = 0,
                                                .array_layer_count = 1,
                                                .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                .x = 0,
                                                .y = 0,
                                                .z = 0,
                                                .width = 1,
                                                .height = 1,
                                                .depth = 1};
  REQUIRE(state.copy_texture_to_buffer(*compute_copy_recorder, *texture, *vertex_buffer,
                                       GRANIT_TEXTURE_FORMAT_RGBA8_UNORM, copy_layout,
                                       copy_region) == GRANIT_SUCCESS);
  REQUIRE(state.end_command_recorder(*compute_copy_recorder) == GRANIT_SUCCESS);
  auto empty_pipeline_layout = state.allocate_pipeline_layout_resource();
  REQUIRE(empty_pipeline_layout != nullptr);
  REQUIRE(state.create_pipeline_layout({}, *empty_pipeline_layout) == GRANIT_SUCCESS);
  const std::array<granit::detail::backend_bind_group_layout_resource*, 1> invalid_pipeline_layouts{
      nullptr};
  auto invalid_pipeline_layout = state.allocate_pipeline_layout_resource();
  REQUIRE(invalid_pipeline_layout != nullptr);
  CHECK(state.create_pipeline_layout(invalid_pipeline_layouts, *invalid_pipeline_layout) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  auto surface = state.allocate_surface_resource();
  REQUIRE(surface != nullptr);
  const auto native_a = reinterpret_cast<void*>(std::uintptr_t{1});
  const auto native_b = reinterpret_cast<void*>(std::uintptr_t{2});
  REQUIRE(state.create_win32_surface(native_a, native_b, *surface) == GRANIT_SUCCESS);
  surface.reset();
  surface = state.allocate_surface_resource();
  REQUIRE(state.create_xcb_surface(native_a, 42, *surface) == GRANIT_SUCCESS);
  surface.reset();
  surface = state.allocate_surface_resource();
  REQUIRE(state.create_wayland_surface(native_a, native_b, *surface) == GRANIT_SUCCESS);
  surface.reset();
  surface = state.allocate_surface_resource();
  REQUIRE(state.create_canvas_surface("#canvas", *surface) == GRANIT_SUCCESS);
  surface.reset();
  CHECK(state.initialize_static(module.api(), surface_types, nullptr, nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}
