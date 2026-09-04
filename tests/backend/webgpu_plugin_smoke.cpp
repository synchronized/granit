// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#include "backend/plugin/plugin_loader.h"
#include "shader_asset.h"

namespace {

std::vector<std::byte> load_file(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  const std::vector<char> input{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> output(input.size());
  for (std::size_t index = 0; index < input.size(); ++index)
    output[index] = static_cast<std::byte>(input[index]);
  return output;
}

void* allocate(uint64_t size, uint64_t alignment, void*) {
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void deallocate(void* memory, uint64_t, uint64_t alignment, void*) {
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category category,
              const char* message, uint32_t message_length, void*) {
  std::fprintf(stderr, "WebGPU 诊断 severity=%u category=%u: %.*s\n",
               static_cast<unsigned>(severity), static_cast<unsigned>(category),
               static_cast<int>(message_length), message == nullptr ? "" : message);
}

bool near_byte(std::uint32_t value, std::uint32_t expected) {
  return value + 1 >= expected && value <= expected + 1;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "用法：granit_webgpu_plugin_smoke <shader.granit-shader>\n");
    return 1;
  }
  const auto asset_bytes = load_file(argv[1]);
  granit::tools::shader_asset_view asset;
  if (granit::tools::decode_shader_asset(asset_bytes, asset) !=
          granit::tools::shader_asset_error::success ||
      asset.wgsl.empty()) {
    std::fprintf(stderr, "读取 WebGPU Shader 资产失败：%s\n", argv[1]);
    return 1;
  }
  const auto smoke_begin = std::chrono::steady_clock::now();
  granit::detail::backend_plugin_loader loader;
  const auto open_result =
      loader.open(GRANIT_WEBGPU_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU);
  if (open_result != GRANIT_SUCCESS) {
    std::fprintf(stderr, "加载 WebGPU 插件失败：%d\n", static_cast<int>(open_result));
    return 1;
  }

  granit_backend_plugin_host_api host{
      sizeof(granit_backend_plugin_host_api), 0, diagnose, nullptr, allocate, deallocate, nullptr};
  granit_backend_plugin_instance instance{};
  const auto create_result = loader.create_instance(&host, &instance);
  if (create_result != GRANIT_SUCCESS || instance == 0) {
    std::fprintf(stderr, "创建 WebGPU 插件实例失败：%d，instance=%llu\n",
                 static_cast<int>(create_result), static_cast<unsigned long long>(instance));
    return 2;
  }
  granit_backend_plugin_capabilities capabilities{};
  capabilities.struct_size = sizeof(capabilities);
  const auto capabilities_result = loader.get_capabilities(instance, &capabilities);
  if (capabilities_result != GRANIT_SUCCESS || capabilities.max_buffer_size == 0 ||
      capabilities.max_texture_dimension_2d == 0 || capabilities.max_bind_groups == 0 ||
      capabilities.max_color_attachments == 0) {
    std::fprintf(stderr, "查询 WebGPU 能力失败：%d\n", static_cast<int>(capabilities_result));
    return 3;
  }

  granit_backend_plugin_buffer_desc buffer_desc{};
  buffer_desc.struct_size = sizeof(buffer_desc);
  buffer_desc.size = 16;
  buffer_desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT |
                      GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  granit_backend_plugin_buffer buffer{};
  const std::uint32_t source[]{1, 2, 3, 4};
  std::uint32_t destination[4]{};
  if (loader.create_buffer(instance, &buffer_desc, &buffer) != GRANIT_SUCCESS || buffer == 0 ||
      loader.write_buffer(instance, buffer, 0, source, sizeof(source)) != GRANIT_SUCCESS ||
      loader.read_buffer(instance, buffer, 0, destination, sizeof(destination)) != GRANIT_SUCCESS ||
      destination[0] != source[0] || destination[1] != source[1] || destination[2] != source[2] ||
      destination[3] != source[3] || loader.destroy_buffer(instance, buffer) != GRANIT_SUCCESS) {
    std::fprintf(stderr, "WebGPU Buffer 写入或回读失败\n");
    return 4;
  }

  granit_backend_plugin_texture_desc texture_desc{};
  texture_desc.struct_size = sizeof(texture_desc);
  texture_desc.width = 16;
  texture_desc.height = 16;
  texture_desc.usage = GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT |
                       GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT;
  texture_desc.format = GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.mip_level_count = 1;
  texture_desc.dimension = GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D;
  texture_desc.array_layer_count = 1;
  const granit_backend_plugin_texture_view_desc texture_view_desc{
      sizeof(granit_backend_plugin_texture_view_desc),
      GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM,
      0,
      1,
      GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D,
      0,
      1};
  granit_backend_plugin_texture texture{};
  granit_backend_plugin_texture_view view{};
  granit_backend_plugin_sampler_desc sampler_desc{};
  sampler_desc.struct_size = sizeof(sampler_desc);
  sampler_desc.min_filter = GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  sampler_desc.mag_filter = GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  sampler_desc.mipmap_filter = GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  sampler_desc.address_mode_u = GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_REPEAT;
  sampler_desc.address_mode_v = GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_REPEAT;
  sampler_desc.address_mode_w = GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_REPEAT;
  sampler_desc.max_anisotropy = 1;
  granit_backend_plugin_sampler sampler{};
  const granit_backend_plugin_bind_group_layout_entry layout_entries[]{
      {0, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE,
       GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT, 1},
      {1, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER, GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT,
       1}};
  const granit_backend_plugin_bind_group_layout_desc layout_desc{
      sizeof(granit_backend_plugin_bind_group_layout_desc), 2, layout_entries, 0};
  granit_backend_plugin_bind_group_layout bind_group_layout{};
  granit_backend_plugin_bind_group bind_group{};
  granit_backend_plugin_pipeline_layout pipeline_layout{};
  granit_backend_plugin_shader vertex_shader{};
  granit_backend_plugin_shader fragment_shader{};
  granit_backend_plugin_render_pipeline pipeline{};
  if (loader.create_texture(instance, &texture_desc, &texture) != GRANIT_SUCCESS || texture == 0 ||
      loader.create_texture_view(instance, texture, &texture_view_desc, &view) != GRANIT_SUCCESS ||
      view == 0 || loader.create_sampler(instance, &sampler_desc, &sampler) != GRANIT_SUCCESS ||
      sampler == 0 ||
      loader.create_bind_group_layout(instance, &layout_desc, &bind_group_layout) !=
          GRANIT_SUCCESS ||
      bind_group_layout == 0) {
    std::fprintf(stderr, "WebGPU Texture、View、Sampler 或绑定布局创建失败\n");
    return 5;
  }
  granit_backend_plugin_bind_group_desc bind_group_desc{};
  const granit_backend_plugin_bind_group_entry group_entries[]{
      {0, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE, 0, view, 0, 0, 0},
      {1, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER, 0, 0, sampler, 0, 0}};
  bind_group_desc.struct_size = sizeof(bind_group_desc);
  bind_group_desc.entry_count = 2;
  bind_group_desc.layout = bind_group_layout;
  bind_group_desc.entries = group_entries;
  granit_backend_plugin_shader_desc vertex_desc{sizeof(granit_backend_plugin_shader_desc),
                                                GRANIT_BACKEND_PLUGIN_SHADER_STAGE_VERTEX,
                                                asset.wgsl.data(),
                                                asset.wgsl.size(),
                                                "vs_main",
                                                7};
  auto fragment_desc = vertex_desc;
  fragment_desc.stage = GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT;
  fragment_desc.wgsl = asset.wgsl.data();
  fragment_desc.wgsl_length = asset.wgsl.size();
  fragment_desc.entry_point = "fs_main";
  const granit_backend_plugin_bind_group_layout pipeline_layouts[]{bind_group_layout};
  const granit_backend_plugin_pipeline_layout_desc pipeline_layout_desc{
      sizeof(granit_backend_plugin_pipeline_layout_desc), 1, pipeline_layouts, 0};
  if (loader.create_bind_group(instance, &bind_group_desc, &bind_group) != GRANIT_SUCCESS ||
      bind_group == 0 ||
      loader.create_pipeline_layout(instance, &pipeline_layout_desc, &pipeline_layout) !=
          GRANIT_SUCCESS ||
      pipeline_layout == 0 ||
      loader.create_shader(instance, &vertex_desc, &vertex_shader) != GRANIT_SUCCESS ||
      loader.create_shader(instance, &fragment_desc, &fragment_shader) != GRANIT_SUCCESS) {
    std::fprintf(stderr, "WebGPU 绑定或 Shader 生命周期验证失败\n");
    return 6;
  }
  granit_backend_plugin_render_pipeline_desc pipeline_desc{
      sizeof(granit_backend_plugin_render_pipeline_desc),
      0,
      pipeline_layout,
      vertex_shader,
      fragment_shader,
      GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM,
      0,
      nullptr,
      0,
      0,
      0,
      GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_ALWAYS,
      0,
      0.0F,
      0.0F,
      0,
      GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE,
      GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ZERO,
      GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_ADD,
      GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE,
      GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ZERO,
      GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_ADD,
      GRANIT_BACKEND_PLUGIN_COLOR_WRITE_ALL_BITS,
      GRANIT_BACKEND_PLUGIN_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      GRANIT_BACKEND_PLUGIN_FRONT_FACE_COUNTER_CLOCKWISE,
      GRANIT_BACKEND_PLUGIN_CULL_MODE_NONE,
      GRANIT_BACKEND_PLUGIN_POLYGON_MODE_FILL,
      1};
  if (loader.create_render_pipeline(instance, &pipeline_desc, &pipeline) != GRANIT_SUCCESS ||
      pipeline == 0) {
    std::fprintf(stderr, "WebGPU 绑定或 Render Pipeline 生命周期验证失败\n");
    return 6;
  }

  auto target_desc = texture_desc;
  target_desc.width = 64;
  target_desc.height = 64;
  target_desc.usage = GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_SRC_BIT |
                      GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT;
  granit_backend_plugin_texture target_texture{};
  granit_backend_plugin_texture_view target_view{};
  granit_backend_plugin_buffer_desc readback_desc{};
  readback_desc.struct_size = sizeof(readback_desc);
  readback_desc.size = 16384;
  readback_desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT |
                        GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  granit_backend_plugin_buffer readback{};
  granit_backend_plugin_command_recorder recorder{};
  granit_backend_plugin_command_buffer command_buffer{};
  std::uint32_t corner{};
  std::uint32_t center{};
  std::chrono::steady_clock::duration submit_duration{};
  const float clear_color[]{0.0F, 0.0F, 0.0F, 1.0F};
  if (loader.create_texture(instance, &target_desc, &target_texture) != GRANIT_SUCCESS ||
      loader.create_texture_view(instance, target_texture, &texture_view_desc, &target_view) !=
          GRANIT_SUCCESS ||
      loader.create_buffer(instance, &readback_desc, &readback) != GRANIT_SUCCESS ||
      loader.create_command_recorder(instance, &recorder) != GRANIT_SUCCESS || recorder == 0 ||
      loader.recorder_begin_rendering(
          instance, recorder, target_view, GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR,
          GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE, clear_color) != GRANIT_SUCCESS ||
      loader.recorder_bind_pipeline(instance, recorder, pipeline) != GRANIT_SUCCESS ||
      loader.recorder_bind_graphics_groups(instance, recorder, pipeline_layout, 0,
                                           std::span{&bind_group, 1}, {}) != GRANIT_SUCCESS ||
      loader.recorder_draw_vertices(instance, recorder, 3, 1, 0, 0) != GRANIT_SUCCESS ||
      loader.recorder_end_rendering(instance, recorder) != GRANIT_SUCCESS ||
      loader.recorder_copy_texture_to_buffer(instance, recorder, target_texture, readback, 64, 64,
                                             256) != GRANIT_SUCCESS ||
      loader.finish_command_recorder(instance, recorder, &command_buffer) != GRANIT_SUCCESS ||
      command_buffer == 0) {
    std::fprintf(stderr, "WebGPU 离屏三角形命令录制失败\n");
    return 7;
  }
  const auto submit_begin = std::chrono::steady_clock::now();
  const auto submit_result = loader.submit_command_buffer(instance, command_buffer);
  submit_duration = std::chrono::steady_clock::now() - submit_begin;
  if (submit_result != GRANIT_SUCCESS ||
      loader.read_buffer(instance, readback, 0, &corner, sizeof(corner)) != GRANIT_SUCCESS ||
      loader.read_buffer(instance, readback, 32 * 256 + 32 * 4, &center, sizeof(center)) !=
          GRANIT_SUCCESS ||
      corner != UINT32_C(0xff000000) || !near_byte(center & UINT32_C(0xff), 51) ||
      !near_byte((center >> 8) & UINT32_C(0xff), 179) ||
      !near_byte((center >> 16) & UINT32_C(0xff), 102) ||
      ((center >> 24) & UINT32_C(0xff)) != 255 ||
      loader.destroy_command_recorder(instance, recorder) != GRANIT_SUCCESS ||
      loader.destroy_buffer(instance, readback) != GRANIT_SUCCESS ||
      loader.destroy_texture_view(instance, target_view) != GRANIT_SUCCESS ||
      loader.destroy_texture(instance, target_texture) != GRANIT_SUCCESS ||
      loader.destroy_render_pipeline(instance, pipeline) != GRANIT_SUCCESS ||
      loader.destroy_shader(instance, vertex_shader) != GRANIT_SUCCESS ||
      loader.destroy_shader(instance, fragment_shader) != GRANIT_SUCCESS ||
      loader.destroy_pipeline_layout(instance, pipeline_layout) != GRANIT_SUCCESS ||
      loader.destroy_bind_group(instance, bind_group) != GRANIT_SUCCESS ||
      loader.destroy_bind_group_layout(instance, bind_group_layout) != GRANIT_SUCCESS ||
      loader.destroy_sampler(instance, sampler) != GRANIT_SUCCESS ||
      loader.destroy_texture_view(instance, view) != GRANIT_SUCCESS ||
      loader.destroy_texture(instance, texture) != GRANIT_SUCCESS) {
    std::fprintf(stderr, "WebGPU 离屏三角形渲染或像素回读验证失败\n");
    return 7;
  }
  const auto destroy_result = loader.destroy_instance(instance);
  if (destroy_result != GRANIT_SUCCESS) {
    std::fprintf(stderr, "销毁 WebGPU 插件实例失败：%d\n", static_cast<int>(destroy_result));
    return 8;
  }
  loader.close();
  if (loader.is_open()) {
    std::fprintf(stderr, "卸载 WebGPU 插件失败\n");
    return 9;
  }
  const auto total_duration = std::chrono::steady_clock::now() - smoke_begin;
  std::fprintf(stdout, "WebGPU smoke：Queue Submit %lld us，总耗时 %lld ms\n",
               static_cast<long long>(
                   std::chrono::duration_cast<std::chrono::microseconds>(submit_duration).count()),
               static_cast<long long>(
                   std::chrono::duration_cast<std::chrono::milliseconds>(total_duration).count()));
  return 0;
}
