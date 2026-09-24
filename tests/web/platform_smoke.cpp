// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/render_pipeline.h>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/pipeline_warmup.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/surface.h>
#include <granit/renderer/swapchain.h>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/texture_asset.h>
#include <granit/renderer/timestamp_query.h>
#include <granit/window/native.hpp>
#include <granit/window/window.hpp>

#include "model_viewer/application_core.h"
#include "model_viewer/render_task_executor.h"
#include "model_viewer/web/application.h"
#include "model_viewer/web/browser_test_hooks.h"
#include "support/renderer_fixture.h"

namespace {

bool validate_texture_asset_contract() {
  std::array<std::byte, 192> manifest{};
  const auto write_u32 = [&manifest](std::size_t offset, std::uint32_t value) {
    for (std::uint32_t index = 0; index < 4; ++index)
      manifest[offset + index] = static_cast<std::byte>(value >> (index * 8U));
  };
  const auto write_u64 = [&manifest](std::size_t offset, std::uint64_t value) {
    for (std::uint32_t index = 0; index < 8; ++index)
      manifest[offset + index] = static_cast<std::byte>(value >> (index * 8U));
  };
  constexpr char magic[] = "GRNTEXA";
  for (std::size_t index = 0; index < sizeof(magic) - 1; ++index)
    manifest[index] = static_cast<std::byte>(magic[index]);
  write_u32(8, 1);
  write_u32(12, 80);
  write_u32(16, 4);
  write_u32(20, 4);
  write_u32(24, 1);
  write_u32(28, 1);
  write_u32(32, 1);
  write_u32(36, GRANIT_TEXTURE_DIMENSION_2D);
  write_u32(40, 1);
  write_u32(44, 1);
  manifest[48] = std::byte{1};
  write_u32(80, GRANIT_TEXTURE_FORMAT_RGBA8_SRGB);
  write_u32(84, GRANIT_TEXTURE_USAGE_SAMPLED_BIT);
  write_u32(92, 1);
  write_u64(104, 64);
  write_u64(168, 64);
  write_u32(176, 16);
  write_u32(180, 4);
  granit_texture_asset_info inspected = GRANIT_TEXTURE_ASSET_INFO_INIT;
  return granit_texture_asset_inspect(manifest.data(), manifest.size(), &inspected) ==
             GRANIT_SUCCESS &&
         inspected.variant_count == 1 && inspected.subresource_count == 1;
}

bool load_startup_resource() noexcept {
  auto* file = std::fopen("/assets/s10d_startup.txt", "rb");
  if (file == nullptr) {
    return false;
  }
  char content[64]{};
  const auto size = std::fread(content, 1, sizeof(content) - 1, file);
  std::fclose(file);
  constexpr char expected[] = "granit-s10d-web-platform";
  return size >= sizeof(expected) - 1 && std::memcmp(content, expected, sizeof(expected) - 1) == 0;
}

std::string load_text_resource(const char* path) {
  auto* file = std::fopen(path, "rb");
  if (file == nullptr)
    return {};
  std::string content;
  std::array<char, 1024> buffer{};
  while (const auto size = std::fread(buffer.data(), 1, buffer.size(), file))
    content.append(buffer.data(), size);
  std::fclose(file);
  return content;
}

bool validate_fixture_assets() {
  const auto vertex = load_text_resource("/assets/dynamic_uniform.vert.wgsl");
  const auto fragment = load_text_resource("/assets/dynamic_uniform.frag.wgsl");
  return !vertex.empty() && !fragment.empty() &&
         granit::test::renderer_fixture::vertices.size() == 4 * 7 &&
         granit::test::renderer_fixture::indices.size() == 6 &&
         granit::test::renderer_fixture::make_uniform_data().size() == 4 * 256;
}

granit_matrix4 identity_matrix() noexcept {
  granit_matrix4 value{};
  value.elements[0] = 1.0F;
  value.elements[5] = 1.0F;
  value.elements[10] = 1.0F;
  value.elements[15] = 1.0F;
  return value;
}

granit_result validate_public_timestamp(granit_renderer renderer,
                                        const granit_renderer_limits& limits) {
  granit_timestamp_query_pool_desc query_desc{sizeof(query_desc), 2, 0};
  granit_timestamp_query_pool pool{};
  auto result = granit_timestamp_query_pool_create(renderer, &query_desc, &pool);
  if ((limits.supported_features & GRANIT_RENDERER_FEATURE_TIMESTAMP_QUERY_BIT) == 0)
    return result == GRANIT_ERROR_UNSUPPORTED ? GRANIT_SUCCESS : GRANIT_ERROR_INTERNAL;
  if (result != GRANIT_SUCCESS)
    return result;

  granit_command_recorder recorder{};
  granit_async_operation operation{};
  const auto cleanup = [&] {
    if (operation != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_async_operation_destroy(renderer, operation));
    if (recorder != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_command_recorder_destroy(renderer, recorder));
    static_cast<void>(granit_timestamp_query_pool_destroy(renderer, pool));
  };
  const granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  result = granit_command_recorder_create(renderer, &recorder_desc, &recorder);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_begin(renderer, recorder);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_reset_timestamp_queries(renderer, recorder, pool, 0, 2);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_write_timestamp(renderer, recorder, pool,
                                                     GRANIT_TIMESTAMP_STAGE_TOP, 0);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_write_timestamp(renderer, recorder, pool,
                                                     GRANIT_TIMESTAMP_STAGE_BOTTOM, 1);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end(renderer, recorder);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_submit(renderer, recorder);
  if (result == GRANIT_SUCCESS)
    result = granit_timestamp_query_pool_get_results_async(renderer, pool, 0, 2, &operation);
  if (result != GRANIT_SUCCESS) {
    cleanup();
    return result;
  }

  granit_async_operation_status status = GRANIT_ASYNC_OPERATION_STATUS_INIT;
  for (std::uint32_t attempt = 0; attempt < 600; ++attempt) {
    result = granit_async_operation_get_status(renderer, operation, &status);
    if (result != GRANIT_SUCCESS || status.state == GRANIT_ASYNC_OPERATION_STATE_FAILED ||
        status.state == GRANIT_ASYNC_OPERATION_STATE_CANCELLED)
      break;
    if (status.state == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED)
      break;
    emscripten_sleep(0);
    static_cast<void>(granit_renderer_process_events(renderer));
  }
  std::array<std::uint64_t, 2> values{};
  if (result == GRANIT_SUCCESS && status.state == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED)
    result = granit_timestamp_query_pool_copy_results(renderer, pool, operation, values.data(),
                                                      static_cast<std::uint32_t>(values.size()));
  else if (result == GRANIT_SUCCESS)
    result = status.result == GRANIT_ERROR_NOT_READY ? GRANIT_ERROR_NOT_READY : status.result;
  if (result == GRANIT_SUCCESS && values[1] < values[0])
    result = GRANIT_ERROR_INTERNAL;
  cleanup();
  return result;
}

granit_result validate_public_transfers(granit_renderer renderer) {
  const auto owner = granit::renderer_ref::from_native(renderer);
  constexpr std::array<std::uint8_t, 16> pixels{1, 2,  3,  4,  5,  6,  7,  8,
                                                9, 10, 11, 12, 13, 14, 15, 16};
  constexpr auto texture_usage =
      granit::texture_usage::transfer_source | granit::texture_usage::transfer_destination;
  granit::buffer upload;
  auto result = upload.initialize(
      owner,
      {.size = pixels.size(),
       .usage = granit::buffer_usage::transfer_source | granit::buffer_usage::transfer_destination,
       .location = granit::memory_location::upload},
      std::as_bytes(std::span{pixels}));
  if (result != granit::result::success)
    return granit::to_native(result);

  granit::texture non_power_of_two_texture;
  result = non_power_of_two_texture.initialize(owner, {.format = granit::texture_format::rgba8_srgb,
                                                       .usage = texture_usage,
                                                       .width = 7,
                                                       .height = 5,
                                                       .mip_levels = 3,
                                                       .array_layers = 1});
  if (result != granit::result::success)
    return granit::to_native(result);
  granit::texture cube_texture;
  result = cube_texture.initialize(owner, {.dimension = granit::texture_dimension::cube,
                                           .format = granit::texture_format::rgba8_unorm,
                                           .usage = texture_usage,
                                           .width = 7,
                                           .height = 7,
                                           .mip_levels = 3,
                                           .array_layers = 6});
  if (result != granit::result::success)
    return granit::to_native(result);

  granit::buffer readback;
  result = readback.initialize(owner, {.size = pixels.size(),
                                       .usage = granit::buffer_usage::transfer_destination,
                                       .location = granit::memory_location::readback});
  if (result != granit::result::success)
    return granit::to_native(result);

  granit::texture source_texture;
  result = source_texture.initialize(owner, {.format = granit::texture_format::rgba8_unorm,
                                             .usage = texture_usage,
                                             .width = 2,
                                             .height = 2,
                                             .mip_levels = 2});
  if (result != granit::result::success)
    return granit::to_native(result);
  granit::texture destination_texture;
  result = destination_texture.initialize(owner, {.format = granit::texture_format::rgba8_unorm,
                                                  .usage = texture_usage,
                                                  .width = 2,
                                                  .height = 2});
  if (result != granit::result::success)
    return granit::to_native(result);

  granit::command_recorder recorder;
  result = recorder.initialize(owner);
  if (result != granit::result::success)
    return granit::to_native(result);
  if ((result = recorder.begin()) != granit::result::success)
    return granit::to_native(result);

  constexpr granit::buffer_copy_region buffer_region{
      .source_offset = 0, .destination_offset = 0, .size = pixels.size()};
  constexpr granit::texture_data_layout layout{};
  granit::texture_write_region texture_region{};
  texture_region.array_layer_count = 1;
  texture_region.aspect = granit::texture_aspect::color;
  texture_region.width = 2;
  texture_region.height = 2;
  texture_region.depth = 1;
  granit::texture_copy_region copy_region{};
  copy_region.array_layer_count = 1;
  copy_region.aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT;
  copy_region.width = 2;
  copy_region.height = 2;
  copy_region.depth = 1;

  result = recorder.copy_buffer(upload.ref(), readback.ref(), std::span{&buffer_region, 1});
  if (result == granit::result::success)
    result = recorder.fill_buffer(upload.ref(), 0, pixels.size(), UINT32_C(0x40302010));
  if (result == granit::result::success)
    result =
        recorder.copy_buffer_to_texture(upload.ref(), source_texture.ref(), layout, texture_region);
  if (result == granit::result::success) {
    const granit::texture_mipmap_range mipmap_range{
        .base_mip_level = 0, .level_count = 2, .base_array_layer = 0, .array_layer_count = 1};
    result = recorder.generate_mipmaps(source_texture.ref(), mipmap_range);
  }
  if (result == granit::result::success) {
    const granit::texture_mipmap_range mipmap_range{
        .base_mip_level = 0, .level_count = 3, .base_array_layer = 0, .array_layer_count = 1};
    result = recorder.generate_mipmaps(non_power_of_two_texture.ref(), mipmap_range);
  }
  if (result == granit::result::success) {
    const granit::texture_mipmap_range mipmap_range{
        .base_mip_level = 0, .level_count = 3, .base_array_layer = 0, .array_layer_count = 6};
    result = recorder.generate_mipmaps(cube_texture.ref(), mipmap_range);
  }
  if (result == granit::result::success)
    result = recorder.copy_texture(source_texture.ref(), destination_texture.ref(), copy_region);
  if (result == granit::result::success)
    result = recorder.copy_texture_to_buffer(destination_texture.ref(), readback.ref(), layout,
                                             texture_region);
  if (result == granit::result::success)
    result = recorder.end();
  if (result == granit::result::success)
    result = recorder.submit();
  if (result == granit::result::success)
    result = recorder.reset();
  return granit::to_native(result);
}

granit_result draw_shared_fixture(granit_renderer renderer, granit_frame frame,
                                  granit_texture_view target_view,
                                  granit_texture_format native_format, std::uint32_t width,
                                  std::uint32_t height) {
  const auto owner = granit::renderer_ref::from_native(renderer);
  const auto vertex_wgsl = load_text_resource("/assets/dynamic_uniform.vert.wgsl");
  const auto fragment_wgsl = load_text_resource("/assets/dynamic_uniform.frag.wgsl");
  granit::shader vertex;
  granit::shader fragment;
  auto result = vertex.initialize(
      owner, {.stage = granit::shader_stage::vertex,
              .code_format = granit::shader_code_format::wgsl,
              .code = std::as_bytes(std::span{vertex_wgsl.data(), vertex_wgsl.size()})});
  if (result != granit::result::success)
    return granit::to_native(result);
  result = fragment.initialize(
      owner, {.stage = granit::shader_stage::fragment,
              .code_format = granit::shader_code_format::wgsl,
              .code = std::as_bytes(std::span{fragment_wgsl.data(), fragment_wgsl.size()})});
  if (result != granit::result::success)
    return granit::to_native(result);

  const std::array declarations{
      granit::bind_group_layout_entry{.binding = 0,
                                      .type = granit::binding_type::dynamic_uniform_buffer,
                                      .visibility = granit::shader_stage_flags::vertex},
      granit::bind_group_layout_entry{.binding = 1,
                                      .type = granit::binding_type::sampled_texture,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = 2,
                                      .type = granit::binding_type::sampler,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = 3,
                                      .type = granit::binding_type::sampled_texture,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = 4,
                                      .type = granit::binding_type::sampled_texture,
                                      .visibility = granit::shader_stage_flags::fragment}};
  granit::bind_group_layout group_layout;
  result = group_layout.initialize(owner, declarations);
  if (result != granit::result::success)
    return granit::to_native(result);
  const auto group_layout_handle = group_layout.ref();
  granit::pipeline_layout pipeline_layout;
  result = pipeline_layout.initialize(owner, std::span{&group_layout_handle, 1});
  if (result != granit::result::success)
    return granit::to_native(result);

  const auto color_format = static_cast<granit::texture_format>(native_format);
  const std::array vertex_attributes{
      granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x2},
      granit::vertex_attribute{
          .location = 1, .format = granit::vertex_format::float32x2, .offset = sizeof(float) * 2},
      granit::vertex_attribute{
          .location = 2, .format = granit::vertex_format::float32x3, .offset = sizeof(float) * 4}};
  const std::array vertex_layouts{
      granit::vertex_buffer_layout{.stride = sizeof(float) * 7, .attributes = vertex_attributes}};
  granit::graphics_pipeline pipeline;
  result = pipeline.initialize(
      owner, {.layout = pipeline_layout.ref(),
              .vertex_shader = vertex.ref(),
              .fragment_shader = fragment.ref(),
              .color_formats = std::span{&color_format, 1},
              .depth_stencil_format = granit::texture_format::d32_float,
              .vertex_buffers = vertex_layouts,
              .primitive = {},
              .depth = granit::depth_state{.test_enabled = true, .write_enabled = true},
              .color_blends = {},
              .depth_bias = std::nullopt});
  if (result != granit::result::success)
    return granit::to_native(result);

  const auto uniform_data = granit::test::renderer_fixture::make_uniform_data();
  granit::buffer uniform;
  result = uniform.initialize(
      owner,
      {.size = uniform_data.size(),
       .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination},
      uniform_data);
  if (result != granit::result::success)
    return granit::to_native(result);

  const granit::texture_desc base_desc{.format = granit::texture_format::rgba8_srgb,
                                       .usage = granit::texture_usage::sampled |
                                                granit::texture_usage::transfer_destination,
                                       .width = 2,
                                       .height = 2};
  const granit::texture_write_region texture_region{.array_layer_count = 1,
                                                    .aspect = granit::texture_aspect::color,
                                                    .width = 2,
                                                    .height = 2,
                                                    .depth = 1};
  granit::texture base_color;
  granit::texture_view base_color_view;
  result = base_color.initialize(owner, base_desc);
  if (result == granit::result::success)
    result = base_color_view.initialize(owner, base_color.ref());
  if (result == granit::result::success)
    result = base_color.write(
        std::as_bytes(std::span{granit::test::renderer_fixture::base_color_pixels}), {},
        texture_region);
  granit::texture normal;
  granit::texture_view normal_view;
  auto material_desc = base_desc;
  material_desc.format = granit::texture_format::rgba8_unorm;
  if (result == granit::result::success)
    result = normal.initialize(owner, material_desc);
  if (result == granit::result::success)
    result = normal_view.initialize(owner, normal.ref());
  if (result == granit::result::success)
    result = normal.write(std::as_bytes(std::span{granit::test::renderer_fixture::normal_pixels}),
                          {}, texture_region);
  granit::texture metallic_roughness;
  granit::texture_view metallic_roughness_view;
  if (result == granit::result::success)
    result = metallic_roughness.initialize(owner, material_desc);
  if (result == granit::result::success)
    result = metallic_roughness_view.initialize(owner, metallic_roughness.ref());
  if (result == granit::result::success)
    result = metallic_roughness.write(
        std::as_bytes(std::span{granit::test::renderer_fixture::metallic_roughness_pixels}), {},
        texture_region);
  if (result != granit::result::success)
    return granit::to_native(result);

  granit::sampler material_sampler;
  result = material_sampler.initialize(owner);
  if (result != granit::result::success)
    return granit::to_native(result);
  const std::array entries{
      granit::bind_group_entry{.binding = 0, .resource = uniform.ref(), .offset = 0, .size = 32},
      granit::bind_group_entry{.binding = 1, .resource = base_color_view.ref()},
      granit::bind_group_entry{.binding = 2, .resource = material_sampler.ref()},
      granit::bind_group_entry{.binding = 3, .resource = normal_view.ref()},
      granit::bind_group_entry{.binding = 4, .resource = metallic_roughness_view.ref()}};
  granit::bind_group group;
  result = group.initialize(owner, group_layout.ref(), entries);
  if (result != granit::result::success)
    return granit::to_native(result);

  constexpr auto& vertices = granit::test::renderer_fixture::vertices;
  constexpr auto& indices = granit::test::renderer_fixture::indices;
  granit::buffer vertex_buffer;
  granit::buffer index_buffer;
  result = vertex_buffer.initialize(
      owner,
      {.size = sizeof(vertices),
       .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination},
      std::as_bytes(std::span{vertices}));
  if (result == granit::result::success)
    result = index_buffer.initialize(
        owner,
        {.size = sizeof(indices),
         .usage = granit::buffer_usage::index | granit::buffer_usage::transfer_destination},
        std::as_bytes(std::span{indices}));

  granit::texture depth_target;
  granit::texture_view depth_view;
  if (result == granit::result::success)
    result =
        depth_target.initialize(owner, {.format = granit::texture_format::d32_float,
                                        .usage = granit::texture_usage::depth_stencil_attachment,
                                        .width = width,
                                        .height = height});
  if (result == granit::result::success)
    result = depth_view.initialize(owner, depth_target.ref());
  if (result != granit::result::success)
    return granit::to_native(result);

  granit_command_recorder recorder{};
  const granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  auto native_result = granit_command_recorder_create(renderer, &recorder_desc, &recorder);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_begin(renderer, recorder);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_bind_graphics_pipeline(renderer, recorder,
                                                                   pipeline.native_handle());
  const granit_viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
  const granit_scissor scissor{0, 0, width, height};
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_set_viewports(renderer, recorder, 0, &viewport, 1);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_set_scissors(renderer, recorder, 0, &scissor, 1);
  const granit_vertex_buffer_binding vertex_binding{vertex_buffer.native_handle(), 0};
  if (native_result == GRANIT_SUCCESS)
    native_result =
        granit_command_recorder_bind_vertex_buffers(renderer, recorder, 0, &vertex_binding, 1);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_bind_index_buffer(
        renderer, recorder, index_buffer.native_handle(), 0, GRANIT_INDEX_TYPE_UINT16);
  const granit_bind_group group_handle = group.native_handle();
  const std::array offsets{UINT32_C(0), UINT32_C(512), UINT32_C(768), UINT32_C(256)};
  granit_bind_groups_desc groups = GRANIT_BIND_GROUPS_DESC_INIT;
  groups.first_group = 0;
  groups.bind_group_count = 1;
  groups.bind_groups = &group_handle;
  groups.dynamic_offset_count = 1;
  groups.dynamic_offsets = offsets.data();
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_bind_graphics_groups(
        renderer, recorder, pipeline_layout.native_handle(), &groups);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = target_view;
  granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth.view = depth_view.native_handle();
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.depth_stencil_attachment = &depth;
  rendering.area = {0, 0, width, height};
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_begin_rendering(renderer, recorder, &rendering);
  for (std::size_t index = 0; native_result == GRANIT_SUCCESS && index < offsets.size(); ++index) {
    if (index != 0) {
      groups.dynamic_offsets = offsets.data() + index;
      native_result = granit_command_recorder_bind_graphics_groups(
          renderer, recorder, pipeline_layout.native_handle(), &groups);
    }
    if (native_result == GRANIT_SUCCESS)
      native_result = granit_command_recorder_draw_indexed(
          renderer, recorder, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
  }
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_end_rendering(renderer, recorder);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_end(renderer, recorder);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_submit_frame(renderer, recorder, frame);
  static_cast<void>(granit_command_recorder_destroy(renderer, recorder));
  return native_result;
}

granit_result validate_presentation(granit_renderer renderer, granit_swapchain swapchain,
                                    const granit_swapchain_info& info) {
  const auto owner = granit::renderer_ref::from_native(renderer);
  granit_result result = GRANIT_SUCCESS;
  // Smoke 目标额外呈现共享 Fixture，正式 Model Viewer 不应向用户暴露测试图形。
  granit_frame frame{};
  std::uint32_t image_index{};
  std::uint32_t needs_recreate{};
  result = granit_swapchain_acquire(renderer, swapchain, &frame, &image_index, &needs_recreate);
  if (result != GRANIT_SUCCESS || frame == GRANIT_NULL_HANDLE) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  granit_texture texture{};
  granit_texture_view view{};
  result = granit_swapchain_get_backbuffer(renderer, swapchain, image_index, &texture, &view);
  if (result != GRANIT_SUCCESS || texture == GRANIT_NULL_HANDLE || view == GRANIT_NULL_HANDLE) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  granit_frame_info frame_info = GRANIT_FRAME_INFO_INIT;
  result = granit_frame_get_info(renderer, frame, &frame_info);
  if (result != GRANIT_SUCCESS || frame_info.frame_slot_count == 0) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  result = granit_frame_cancel(renderer, swapchain, frame, &needs_recreate);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  if (granit_frame_get_info(renderer, frame, &frame_info) != GRANIT_ERROR_INVALID_HANDLE ||
      granit_swapchain_get_backbuffer(renderer, swapchain, image_index, &texture, &view) !=
          GRANIT_ERROR_INVALID_ARGUMENT) {
    return GRANIT_ERROR_INTERNAL;
  }
  result = granit_swapchain_acquire(renderer, swapchain, &frame, &image_index, &needs_recreate);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  result = granit_swapchain_get_backbuffer(renderer, swapchain, image_index, &texture, &view);
  if (result == GRANIT_SUCCESS) {
    result = draw_shared_fixture(renderer, frame, view, info.format, info.width, info.height);
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_frame_cancel(renderer, swapchain, frame, &needs_recreate));
    return result;
  }
  result = granit_swapchain_present(renderer, swapchain, frame, &needs_recreate);
  if (result != GRANIT_SUCCESS ||
      granit_frame_get_info(renderer, frame, &frame_info) != GRANIT_ERROR_INVALID_HANDLE) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INTERNAL : result;
  }

  granit::render_pipeline empty_pipeline;
  auto wrapper_result = empty_pipeline.initialize(owner);
  granit_scene_view scene_view{};
  scene_view.view = identity_matrix();
  scene_view.projection = identity_matrix();
  scene_view.view_projection = identity_matrix();
  scene_view.viewport_width = static_cast<float>(info.width);
  scene_view.viewport_height = static_cast<float>(info.height);
  scene_view.layer_mask = UINT64_MAX;
  granit::scene_snapshot empty_scene;
  if (wrapper_result.ok())
    wrapper_result = empty_scene.initialize(owner, {.views = std::span{&scene_view, 1}});

  granit::texture canvas_texture;
  granit::texture_view canvas_texture_view;
  granit::sampler canvas_sampler;
  if (wrapper_result.ok()) {
    wrapper_result =
        canvas_texture.initialize(owner, {.format = granit::texture_format::rgba8_unorm,
                                          .usage = granit::texture_usage::sampled |
                                                   granit::texture_usage::transfer_destination});
  }
  if (wrapper_result.ok())
    wrapper_result = canvas_texture_view.initialize(owner, canvas_texture.ref());
  constexpr std::array<std::uint8_t, 4> green_pixel{0, 255, 0, 255};
  if (wrapper_result.ok())
    wrapper_result = canvas_texture.write(std::as_bytes(std::span{green_pixel}), {}, {});
  if (wrapper_result.ok())
    wrapper_result = canvas_sampler.initialize(owner);
  granit::canvas_draw_list canvas;
  if (wrapper_result.ok())
    wrapper_result = canvas.initialize(owner);
  const granit::canvas_rect_desc rect{
      .x = 8.0F,
      .y = 8.0F,
      .width = 48.0F,
      .height = 48.0F,
      .state = {.texture = canvas_texture_view.ref(), .sampler = canvas_sampler.ref(), .clip = {}}};
  if (wrapper_result.ok())
    wrapper_result = canvas.append_rect(rect);
  if (wrapper_result.failed())
    return granit::to_native(wrapper_result);

  result = granit_swapchain_acquire(renderer, swapchain, &frame, &image_index, &needs_recreate);
  if (result != GRANIT_SUCCESS)
    return result;
  result = granit_swapchain_get_backbuffer(renderer, swapchain, image_index, &texture, &view);
  if (result == GRANIT_SUCCESS) {
    granit_render_pipeline_render_desc render_desc = GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT;
    render_desc.scene = empty_scene.native_handle();
    render_desc.output = view;
    render_desc.output_format = info.format;
    render_desc.width = info.width;
    render_desc.height = info.height;
    render_desc.frame = frame;
    render_desc.canvas = canvas.native_handle();
    render_desc.clear_color = {0.05F, 0.1F, 0.2F, 1.0F};
    result = granit_render_pipeline_render(renderer, empty_pipeline.native_handle(), &render_desc);
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_frame_cancel(renderer, swapchain, frame, &needs_recreate));
    return result;
  }
  result = granit_swapchain_present(renderer, swapchain, frame, &needs_recreate);
  if (result == GRANIT_SUCCESS)
    std::printf("GRANIT_EMPTY_FRAME:ready\n");
  return result;
}

granit_result validate_renderer(granit_renderer renderer, const granit_renderer_limits& limits) {
  const auto result = validate_public_timestamp(renderer, limits);
  return result == GRANIT_SUCCESS ? validate_public_transfers(renderer) : result;
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_validate_multi_window() noexcept {
  granit::window_system system;
  auto result = system.initialize();
  if (result.failed())
    return granit::to_native(result);

  const granit::window_desc secondary_desc{
      .title = "Secondary",
      .width = 320,
      .height = 180,
      .flags = granit::window_flag::visible | granit::window_flag::resizable,
      .target = granit::window_target::canvas("#secondary-canvas"),
  };
  granit::window secondary;
  result = secondary.initialize(system, secondary_desc);
  if (result.failed())
    return granit::to_native(result);

  granit::window_state state;
  result = secondary.get_state(state);
  if (result.failed() || state.width != 320 || state.height != 180) {
    std::fprintf(stderr, "GRANIT_WINDOW_TARGET:state:%d:%u:%u\n", granit::to_native(result),
                 state.width, state.height);
    return GRANIT_ERROR_INTERNAL;
  }

  granit::window_native_emscripten native = GRANIT_WINDOW_NATIVE_EMSCRIPTEN_INIT;
  result = granit::get_native(system, secondary.ref(), native);
  constexpr std::string_view expected_selector = "#secondary-canvas";
  if (result.failed() || native.canvas_selector_length != expected_selector.size() ||
      std::string_view{native.canvas_selector, native.canvas_selector_length} !=
          expected_selector) {
    std::fprintf(stderr, "GRANIT_WINDOW_TARGET:native:%d:%u\n", granit::to_native(result),
                 native.canvas_selector_length);
    return GRANIT_ERROR_INTERNAL;
  }

  granit::window duplicate;
  const auto duplicate_result = duplicate.initialize(system, secondary_desc);
  if (duplicate_result != granit::result::resource_in_use) {
    std::fprintf(stderr, "GRANIT_WINDOW_TARGET:duplicate:%d\n",
                 granit::to_native(duplicate_result));
    return GRANIT_ERROR_INTERNAL;
  }

  granit::window automatic;
  const granit::window_desc automatic_desc{
      .title = "Automatic",
      .width = 64,
      .height = 64,
      .flags = granit::window_flag::visible | granit::window_flag::resizable,
      .target = {},
  };
  const auto automatic_result = automatic.initialize(system, automatic_desc);
  if (automatic_result != granit::result::resource_in_use) {
    std::fprintf(stderr, "GRANIT_WINDOW_TARGET:automatic:%d\n",
                 granit::to_native(automatic_result));
    return GRANIT_ERROR_INTERNAL;
  }

  result = secondary.reset();
  if (result.failed())
    return granit::to_native(result);
  result = secondary.initialize(system, secondary_desc);
  return granit::to_native(result);
}

int main() {
  if (!load_startup_resource() || !validate_fixture_assets() ||
      !validate_texture_asset_contract()) {
    std::fprintf(stderr, "GRANIT_STATUS:failed:preloaded-resource:%d\n",
                 GRANIT_ERROR_INITIALIZATION_FAILED);
    return 1;
  }
  granit::example::model_viewer::web::browser_test::configure(
      {.renderer_ready = validate_renderer, .presentation_ready = validate_presentation});
  return granit::example::model_viewer::web::run_application(
      {.default_model_url = "model_viewer_fixture.gltf"});
}
