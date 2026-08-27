// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

#include "shader_asset.h"

namespace {

constexpr std::uint32_t render_size = 64;

std::vector<std::byte> load_file(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  const std::vector<char> input{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> output(input.size());
  for (std::size_t index = 0; index < input.size(); ++index)
    output[index] = static_cast<std::byte>(input[index]);
  return output;
}

bool near_byte(std::uint8_t value, std::uint8_t expected) {
  return value + 1 >= expected && value <= expected + 1;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "用法：granit_shader_asset_vulkan_smoke <vertex.granit-shader> "
                 "<fragment.granit-shader>\n";
    return 1;
  }

  const auto vertex_bytes = load_file(argv[1]);
  const auto fragment_bytes = load_file(argv[2]);
  granit::tools::shader_asset_view vertex_asset;
  granit::tools::shader_asset_view fragment_asset;
  if (granit::tools::decode_shader_asset(vertex_bytes, vertex_asset) !=
          granit::tools::shader_asset_error::success ||
      granit::tools::decode_shader_asset(fragment_bytes, fragment_asset) !=
          granit::tools::shader_asset_error::success ||
      vertex_asset.spirv.empty() || fragment_asset.spirv.empty()) {
    std::cerr << "读取 Vulkan Shader 资产失败\n";
    return 1;
  }

  granit::renderer renderer;
  auto result = renderer.initialize(
      {.application_name = "Granit S-10C6 Vulkan Smoke", .enable_validation = true});
  if (granit::failed(result)) {
    std::cerr << "创建 Vulkan Renderer 失败：" << granit::result_message(result) << '\n';
    return 2;
  }

  granit::shader vertex;
  granit::shader fragment;
  granit::pipeline_layout layout;
  granit::graphics_pipeline pipeline;
  result = vertex.initialize(renderer.native_handle(),
                             {.stage = granit::shader_stage::vertex,
                              .code = vertex_asset.spirv});
  if (granit::succeeded(result))
    result = fragment.initialize(renderer.native_handle(),
                                 {.stage = granit::shader_stage::fragment,
                                  .code = fragment_asset.spirv});
  if (granit::succeeded(result))
    result = layout.initialize(renderer.native_handle());
  const granit::texture_format format = granit::texture_format::rgba8_unorm;
  if (granit::succeeded(result)) {
    result = pipeline.initialize(renderer.native_handle(),
                                 {.layout = layout.native_handle(),
                                  .vertex_shader = vertex.native_handle(),
                                  .fragment_shader = fragment.native_handle(),
                                  .color_formats = std::span{&format, 1},
                                  .vertex_buffers = {},
                                  .primitive = {},
                                  .depth = {},
                                  .color_blends = {},
                                  .depth_bias = std::nullopt});
  }

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage =
      GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT;
  texture_desc.width = render_size;
  texture_desc.height = render_size;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  if (granit::succeeded(result)) {
    result = granit::from_native(granit_texture_create_with_default_view(
        renderer.native_handle(), &texture_desc, &texture, &view));
  }

  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  if (granit::succeeded(result))
    result = recorder.begin();
  if (granit::succeeded(result))
    result = recorder.bind_graphics_pipeline(pipeline.native_handle());
  const granit::viewport viewport{0, 0, render_size, render_size, 0, 1};
  const granit::scissor scissor{0, 0, render_size, render_size};
  if (granit::succeeded(result))
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (granit::succeeded(result))
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  const granit::color_attachment_desc color{.view = view,
                                             .clear_value = {0, 0, 0, 1}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, render_size, render_size}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(rendering);
  if (granit::succeeded(result))
    result = recorder.draw(3);
  if (granit::succeeded(result))
    result = recorder.end_rendering();
  if (granit::succeeded(result))
    result = recorder.end();
  if (granit::succeeded(result))
    result = recorder.submit();
  if (granit::succeeded(result))
    result = recorder.reset();

  constexpr std::uint64_t readback_size = render_size * render_size * 4;
  granit::buffer readback;
  granit::command_recorder copy_recorder;
  if (granit::succeeded(result)) {
    result = readback.initialize(renderer.native_handle(),
                                 {.size = readback_size,
                                  .usage = granit::buffer_usage::transfer_destination,
                                  .location = granit::memory_location::readback});
  }
  if (granit::succeeded(result))
    result = copy_recorder.initialize(renderer.native_handle());
  if (granit::succeeded(result))
    result = copy_recorder.begin();
  const granit_texture_data_layout data_layout{};
  const granit_texture_write_region region{.mip_level = 0,
                                            .base_array_layer = 0,
                                            .array_layer_count = 1,
                                            .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                            .x = 0,
                                            .y = 0,
                                            .z = 0,
                                            .width = render_size,
                                            .height = render_size,
                                            .depth = 1};
  if (granit::succeeded(result)) {
    result = copy_recorder.copy_texture_to_buffer(texture, readback.native_handle(), data_layout,
                                                  region);
  }
  if (granit::succeeded(result))
    result = copy_recorder.end();
  if (granit::succeeded(result))
    result = copy_recorder.submit();
  if (granit::succeeded(result))
    result = copy_recorder.reset();

  void* mapped = nullptr;
  if (granit::succeeded(result))
    result = readback.map(0, readback_size, &mapped);
  std::array<std::uint8_t, 4> corner{};
  std::array<std::uint8_t, 4> center{};
  if (granit::succeeded(result)) {
    const auto* pixels = static_cast<const std::uint8_t*>(mapped);
    std::memcpy(corner.data(), pixels, corner.size());
    std::memcpy(center.data(), pixels + (render_size / 2 * render_size + render_size / 2) * 4,
                center.size());
    result = readback.unmap();
  }

  if (view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), view));
  if (texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), texture));
  if (granit::failed(result) || corner != std::array<std::uint8_t, 4>{0, 0, 0, 255} ||
      !near_byte(center[0], 51) || !near_byte(center[1], 179) ||
      !near_byte(center[2], 102) || center[3] != 255) {
    std::cerr << "Vulkan Shader 资产离屏像素验证失败，center="
              << static_cast<unsigned>(center[0]) << ',' << static_cast<unsigned>(center[1]) << ','
              << static_cast<unsigned>(center[2]) << ',' << static_cast<unsigned>(center[3]) << '\n';
    return 3;
  }
  std::cout << "Vulkan Shader 资产离屏像素验证通过\n";
  return 0;
}
