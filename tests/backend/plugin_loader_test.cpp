// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <algorithm>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "backend/plugin_loader.h"

namespace {

constexpr std::string_view vertex_wgsl =
    R"(@vertex fn vs_main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  var positions = array<vec2f, 3>(vec2f(0.0, -0.7), vec2f(0.7, 0.7), vec2f(-0.7, 0.7));
  return vec4f(positions[index], 0.0, 1.0);
})";
constexpr std::string_view fragment_wgsl = R"(@fragment fn fs_main() -> @location(0) vec4f {
  return vec4f(0.2, 0.7, 0.4, 1.0);
})";

struct host_state {
  std::uint32_t allocations{};
  std::uint32_t deallocations{};
  std::uint32_t diagnostics{};
  bool fail_allocation{};
  bool throw_diagnostic{};
};

void* allocate(uint64_t size, uint64_t alignment, void* user_data) {
  auto& state = *static_cast<host_state*>(user_data);
  ++state.allocations;
  if (state.fail_allocation) {
    return nullptr;
  }
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void deallocate(void* memory, uint64_t, uint64_t alignment, void* user_data) {
  auto& state = *static_cast<host_state*>(user_data);
  ++state.deallocations;
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void diagnose(granit_diagnostic_severity, granit_diagnostic_category, const char*, uint32_t,
              void* user_data) {
  auto& state = *static_cast<host_state*>(user_data);
  ++state.diagnostics;
  if (state.throw_diagnostic) {
    throw std::runtime_error{"测试回调异常"};
  }
}

granit_backend_plugin_host_api make_host(host_state& state) {
  return {
      sizeof(granit_backend_plugin_host_api), 0, diagnose, &state, allocate, deallocate, &state};
}

} // namespace

TEST_CASE("后端插件 Loader 区分缺失库和不兼容 ABI", "[backend][plugin]") {
  granit::detail::backend_plugin_loader loader;

  CHECK(loader.open(nullptr, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.open("granit-plugin-that-does-not-exist", GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
        GRANIT_ERROR_BACKEND_UNAVAILABLE);
  CHECK_FALSE(loader.is_open());

  CHECK(loader.open(GRANIT_INCOMPATIBLE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
        GRANIT_ERROR_INCOMPATIBLE_DRIVER);
  CHECK_FALSE(loader.is_open());
}

TEST_CASE("后端插件 Loader 完成版本化握手", "[backend][plugin]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  REQUIRE(loader.api() != nullptr);
  CHECK(loader.api()->abi_version == GRANIT_BACKEND_PLUGIN_ABI_VERSION);
  CHECK(loader.api()->kind == GRANIT_BACKEND_PLUGIN_KIND_WEBGPU);
  REQUIRE(loader.api()->instance_api != nullptr);
  CHECK(loader.api()->instance_api->struct_size >= sizeof(granit_backend_plugin_instance_api));
  CHECK(std::string_view{loader.api()->name, loader.api()->name_length} == "Granit WebGPU (Dawn)");

  host_state state;
  auto host = make_host(state);
  granit_backend_plugin_instance instance{};
  CHECK(loader.create_instance(nullptr, &instance) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.create_instance(&host, nullptr) == GRANIT_ERROR_INVALID_ARGUMENT);
  host.struct_size = 0;
  CHECK(loader.create_instance(&host, &instance) == GRANIT_ERROR_INVALID_ARGUMENT);
  host = make_host(state);
  host.deallocate = nullptr;
  CHECK(loader.create_instance(&host, &instance) == GRANIT_ERROR_INVALID_ARGUMENT);

  host = make_host(state);
  state.fail_allocation = true;
  CHECK(loader.create_instance(&host, &instance) == GRANIT_ERROR_OUT_OF_MEMORY);
  CHECK(instance == 0);
  state.fail_allocation = false;

  state.throw_diagnostic = true;
  CHECK(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);
  CHECK(instance != 0);
  granit_backend_plugin_instance_status status{};
  status.struct_size = sizeof(status);
  CHECK(loader.get_instance_status(instance, &status) == GRANIT_SUCCESS);
  CHECK(status.state == GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_READY);
  CHECK(status.failure_result == GRANIT_SUCCESS);
  CHECK(loader.process_events(instance) == GRANIT_SUCCESS);

  status.struct_size = 0;
  CHECK(loader.get_instance_status(instance, &status) == GRANIT_ERROR_INVALID_ARGUMENT);
  status = {};
  status.struct_size = sizeof(status);
  status.reserved = 1;
  CHECK(loader.get_instance_status(instance, &status) == GRANIT_ERROR_INVALID_ARGUMENT);
  status = {};
  status.struct_size = sizeof(status);
  CHECK(loader.get_instance_status(instance + 1, &status) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(loader.process_events(instance + 1) == GRANIT_ERROR_INVALID_HANDLE);
  granit_backend_plugin_capabilities capabilities{};
  capabilities.struct_size = sizeof(capabilities);
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_SUCCESS);
  CHECK(capabilities.uniform_buffer_offset_alignment == 256);
  CHECK(capabilities.storage_buffer_offset_alignment == 256);
  CHECK(capabilities.max_uniform_buffer_binding_size == 65536);
  CHECK(capabilities.max_storage_buffer_binding_size == 134217728);
  CHECK(capabilities.max_buffer_size == 268435456);
  CHECK(capabilities.max_texture_dimension_2d == 8192);
  CHECK(capabilities.max_bind_groups == 4);
  CHECK(capabilities.max_color_attachments == 8);

  capabilities.struct_size = 0;
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_INVALID_ARGUMENT);
  capabilities = {};
  capabilities.struct_size = sizeof(capabilities);
  capabilities.reserved = 1;
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_INVALID_ARGUMENT);
  capabilities = {};
  capabilities.struct_size = sizeof(capabilities);
  CHECK(loader.get_capabilities(instance + 1, &capabilities) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(loader.destroy_instance(instance) == GRANIT_SUCCESS);
  CHECK(loader.get_instance_status(instance, &status) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(loader.process_events(instance) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(state.allocations == 2);
  CHECK(state.deallocations == 1);
  state.throw_diagnostic = false;

  host.struct_size += 32;
  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);
  CHECK(instance != 0);
  CHECK(state.allocations == 3);
  CHECK(state.diagnostics == 5);

  CHECK(loader.destroy_instance(instance) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(instance) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(state.deallocations == 2);
  CHECK(state.diagnostics == 6);

  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);

  loader.close();
  CHECK_FALSE(loader.is_open());
  CHECK(loader.api() == nullptr);
  CHECK(state.allocations == 4);
  CHECK(state.deallocations == 3);
  CHECK(state.diagnostics == 9);
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("WebGPU 插件 Buffer 遵守所有权、Usage 与范围契约", "[backend][plugin]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  host_state state;
  auto host = make_host(state);
  granit_backend_plugin_instance first{};
  granit_backend_plugin_instance second{};
  REQUIRE(loader.create_instance(&host, &first) == GRANIT_SUCCESS);
  REQUIRE(loader.create_instance(&host, &second) == GRANIT_SUCCESS);

  granit_backend_plugin_buffer_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.size = 16;
  desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT |
               GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  granit_backend_plugin_buffer buffer{};
  REQUIRE(loader.create_buffer(first, &desc, &buffer) == GRANIT_SUCCESS);
  REQUIRE(buffer != 0);

  const std::uint32_t source[]{1, 2, 3, 4};
  std::uint32_t destination[4]{};
  CHECK(loader.write_buffer(first, buffer, 0, source, sizeof(source)) == GRANIT_SUCCESS);
  CHECK(loader.read_buffer(first, buffer, 0, destination, sizeof(destination)) == GRANIT_SUCCESS);
  CHECK(std::equal(std::begin(source), std::end(source), std::begin(destination)));

  CHECK(loader.write_buffer(first, buffer, 14, source, sizeof(std::uint32_t)) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.read_buffer(first, buffer, 8, destination, 12) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.write_buffer(second, buffer, 0, source, sizeof(source)) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(loader.destroy_buffer(second, buffer) == GRANIT_ERROR_INVALID_HANDLE);

  const auto valid_buffer = buffer;
  granit_backend_plugin_buffer invalid_buffer = 123;
  granit_backend_plugin_buffer_desc invalid = desc;
  invalid.struct_size = 0;
  CHECK(loader.create_buffer(first, &invalid, &invalid_buffer) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(invalid_buffer == 0);
  invalid = desc;
  invalid.usage |= GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT;
  CHECK(loader.create_buffer(first, &invalid, &invalid_buffer) == GRANIT_ERROR_INVALID_ARGUMENT);

  REQUIRE(loader.destroy_buffer(first, valid_buffer) == GRANIT_SUCCESS);
  CHECK(loader.destroy_buffer(first, valid_buffer) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(loader.read_buffer(first, valid_buffer, 0, destination, sizeof(destination)) ==
        GRANIT_ERROR_INVALID_HANDLE);

  desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT;
  REQUIRE(loader.create_buffer(first, &desc, &buffer) == GRANIT_SUCCESS);
  CHECK(loader.write_buffer(first, buffer, 0, source, sizeof(source)) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.read_buffer(first, buffer, 0, destination, sizeof(destination)) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  CHECK(loader.destroy_instance(first) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(second) == GRANIT_SUCCESS);
  CHECK(state.allocations == state.deallocations);
}

TEST_CASE("WebGPU 插件 Texture、View 与 Sampler 遵守所有权契约", "[backend][plugin]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  host_state state;
  auto host = make_host(state);
  granit_backend_plugin_instance first{};
  granit_backend_plugin_instance second{};
  REQUIRE(loader.create_instance(&host, &first) == GRANIT_SUCCESS);
  REQUIRE(loader.create_instance(&host, &second) == GRANIT_SUCCESS);

  granit_backend_plugin_texture_desc texture_desc{};
  texture_desc.struct_size = sizeof(texture_desc);
  texture_desc.width = 64;
  texture_desc.height = 32;
  texture_desc.usage = GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT |
                       GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT;
  granit_backend_plugin_texture texture{};
  REQUIRE(loader.create_texture(first, &texture_desc, &texture) == GRANIT_SUCCESS);
  REQUIRE(texture != 0);

  granit_backend_plugin_texture_view view{};
  REQUIRE(loader.create_texture_view(first, texture, &view) == GRANIT_SUCCESS);
  REQUIRE(view != 0);
  granit_backend_plugin_texture_view foreign_view = 123;
  CHECK(loader.create_texture_view(second, texture, &foreign_view) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(foreign_view == 0);
  CHECK(loader.destroy_texture(first, texture) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_texture_view(second, view) == GRANIT_ERROR_INVALID_HANDLE);

  granit_backend_plugin_sampler_desc sampler_desc{};
  sampler_desc.struct_size = sizeof(sampler_desc);
  sampler_desc.min_filter = GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  sampler_desc.mag_filter = GRANIT_BACKEND_PLUGIN_FILTER_NEAREST;
  granit_backend_plugin_sampler sampler{};
  REQUIRE(loader.create_sampler(first, &sampler_desc, &sampler) == GRANIT_SUCCESS);
  REQUIRE(sampler != 0);
  CHECK(loader.destroy_sampler(second, sampler) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.destroy_sampler(first, sampler) == GRANIT_SUCCESS);
  CHECK(loader.destroy_sampler(first, sampler) == GRANIT_ERROR_INVALID_HANDLE);

  REQUIRE(loader.destroy_texture_view(first, view) == GRANIT_SUCCESS);
  CHECK(loader.destroy_texture_view(first, view) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.destroy_texture(first, texture) == GRANIT_SUCCESS);
  CHECK(loader.destroy_texture(first, texture) == GRANIT_ERROR_INVALID_HANDLE);

  auto invalid_texture = texture_desc;
  invalid_texture.width = 0;
  texture = 123;
  CHECK(loader.create_texture(first, &invalid_texture, &texture) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(texture == 0);
  invalid_texture = texture_desc;
  invalid_texture.usage = UINT32_C(0x80000000);
  CHECK(loader.create_texture(first, &invalid_texture, &texture) == GRANIT_ERROR_INVALID_ARGUMENT);

  auto invalid_sampler = sampler_desc;
  invalid_sampler.min_filter = 0;
  sampler = 123;
  CHECK(loader.create_sampler(first, &invalid_sampler, &sampler) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(sampler == 0);

  REQUIRE(loader.create_texture(first, &texture_desc, &texture) == GRANIT_SUCCESS);
  REQUIRE(loader.create_texture_view(first, texture, &view) == GRANIT_SUCCESS);
  REQUIRE(loader.create_sampler(first, &sampler_desc, &sampler) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(first) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(second) == GRANIT_SUCCESS);
  CHECK(state.allocations == state.deallocations);
}

TEST_CASE("WebGPU 插件绑定与 Pipeline 遵守依赖生命周期", "[backend][plugin]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  host_state state;
  auto host = make_host(state);
  granit_backend_plugin_instance first{};
  granit_backend_plugin_instance second{};
  REQUIRE(loader.create_instance(&host, &first) == GRANIT_SUCCESS);
  REQUIRE(loader.create_instance(&host, &second) == GRANIT_SUCCESS);

  granit_backend_plugin_texture_desc texture_desc{};
  texture_desc.struct_size = sizeof(texture_desc);
  texture_desc.width = 16;
  texture_desc.height = 16;
  texture_desc.usage = GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT |
                       GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT;
  granit_backend_plugin_texture texture{};
  granit_backend_plugin_texture_view view{};
  REQUIRE(loader.create_texture(first, &texture_desc, &texture) == GRANIT_SUCCESS);
  REQUIRE(loader.create_texture_view(first, texture, &view) == GRANIT_SUCCESS);
  granit_backend_plugin_sampler_desc sampler_desc{};
  sampler_desc.struct_size = sizeof(sampler_desc);
  sampler_desc.min_filter = GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  sampler_desc.mag_filter = GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  granit_backend_plugin_sampler sampler{};
  REQUIRE(loader.create_sampler(first, &sampler_desc, &sampler) == GRANIT_SUCCESS);

  granit_backend_plugin_bind_group_layout bind_group_layout{};
  REQUIRE(loader.create_bind_group_layout(first, &bind_group_layout) == GRANIT_SUCCESS);
  granit_backend_plugin_bind_group_desc bind_group_desc{};
  bind_group_desc.struct_size = sizeof(bind_group_desc);
  bind_group_desc.layout = bind_group_layout;
  bind_group_desc.texture_view = view;
  bind_group_desc.sampler = sampler;
  granit_backend_plugin_bind_group bind_group{};
  REQUIRE(loader.create_bind_group(first, &bind_group_desc, &bind_group) == GRANIT_SUCCESS);
  granit_backend_plugin_bind_group foreign_bind_group = 123;
  CHECK(loader.create_bind_group(second, &bind_group_desc, &foreign_bind_group) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(foreign_bind_group == 0);

  granit_backend_plugin_pipeline_layout pipeline_layout{};
  REQUIRE(loader.create_pipeline_layout(first, bind_group_layout, &pipeline_layout) ==
          GRANIT_SUCCESS);
  granit_backend_plugin_shader_desc vertex_desc{sizeof(granit_backend_plugin_shader_desc),
                                                GRANIT_BACKEND_PLUGIN_SHADER_STAGE_VERTEX,
                                                vertex_wgsl.data(),
                                                vertex_wgsl.size(),
                                                "vs_main",
                                                7};
  auto fragment_desc = vertex_desc;
  fragment_desc.stage = GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT;
  fragment_desc.wgsl = fragment_wgsl.data();
  fragment_desc.wgsl_length = fragment_wgsl.size();
  fragment_desc.entry_point = "fs_main";
  granit_backend_plugin_shader vertex_shader{};
  granit_backend_plugin_shader fragment_shader{};
  auto invalid_shader_desc = vertex_desc;
  invalid_shader_desc.stage = 0;
  granit_backend_plugin_shader invalid_shader = 123;
  CHECK(loader.create_shader(first, &invalid_shader_desc, &invalid_shader) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(invalid_shader == 0);
  REQUIRE(loader.create_shader(first, &vertex_desc, &vertex_shader) == GRANIT_SUCCESS);
  REQUIRE(loader.create_shader(first, &fragment_desc, &fragment_shader) == GRANIT_SUCCESS);
  granit_backend_plugin_render_pipeline_desc pipeline_desc{
      sizeof(granit_backend_plugin_render_pipeline_desc), 0, pipeline_layout, vertex_shader,
      fragment_shader};
  granit_backend_plugin_render_pipeline pipeline{};
  REQUIRE(loader.create_render_pipeline(first, &pipeline_desc, &pipeline) == GRANIT_SUCCESS);
  granit_backend_plugin_render_pipeline foreign_pipeline = 123;
  CHECK(loader.create_render_pipeline(second, &pipeline_desc, &foreign_pipeline) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(foreign_pipeline == 0);

  auto target_desc = texture_desc;
  target_desc.usage = GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_SRC_BIT |
                      GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT;
  granit_backend_plugin_texture target_texture{};
  granit_backend_plugin_texture_view target_view{};
  REQUIRE(loader.create_texture(first, &target_desc, &target_texture) == GRANIT_SUCCESS);
  REQUIRE(loader.create_texture_view(first, target_texture, &target_view) == GRANIT_SUCCESS);

  granit_backend_plugin_buffer_desc buffer_desc{};
  buffer_desc.struct_size = sizeof(buffer_desc);
  buffer_desc.size = 4096;
  buffer_desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT;
  granit_backend_plugin_buffer buffer{};
  REQUIRE(loader.create_buffer(first, &buffer_desc, &buffer) == GRANIT_SUCCESS);
  granit_backend_plugin_command_recorder recorder{};
  REQUIRE(loader.create_command_recorder(first, &recorder) == GRANIT_SUCCESS);
  CHECK(loader.recorder_copy_buffer_to_texture(first, recorder, buffer, texture, 16, 16, 64) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(loader.recorder_copy_buffer_to_texture(first, recorder, buffer, texture, 16, 16, 256) ==
          GRANIT_SUCCESS);
  REQUIRE(loader.recorder_draw(first, recorder, target_view, pipeline, bind_group) ==
          GRANIT_SUCCESS);
  granit_backend_plugin_buffer_desc readback_desc{};
  readback_desc.struct_size = sizeof(readback_desc);
  readback_desc.size = 4096;
  readback_desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT |
                        GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  granit_backend_plugin_buffer readback{};
  REQUIRE(loader.create_buffer(first, &readback_desc, &readback) == GRANIT_SUCCESS);
  REQUIRE(loader.recorder_copy_texture_to_buffer(first, recorder, target_texture, readback, 16, 16,
                                                 256) == GRANIT_SUCCESS);
  granit_backend_plugin_command_buffer command_buffer{};
  REQUIRE(loader.finish_command_recorder(first, recorder, &command_buffer) == GRANIT_SUCCESS);
  CHECK(command_buffer != 0);
  granit_backend_plugin_command_buffer duplicate_finish = 123;
  CHECK(loader.finish_command_recorder(first, recorder, &duplicate_finish) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(duplicate_finish == 0);
  CHECK(loader.recorder_draw(first, recorder, target_view, pipeline, bind_group) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.submit_command_buffer(second, command_buffer) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.submit_command_buffer(first, command_buffer) == GRANIT_SUCCESS);
  CHECK(loader.submit_command_buffer(first, command_buffer) == GRANIT_ERROR_INVALID_HANDLE);
  std::uint32_t corner{};
  std::uint32_t center{};
  REQUIRE(loader.read_buffer(first, readback, 0, &corner, sizeof(corner)) == GRANIT_SUCCESS);
  REQUIRE(loader.read_buffer(first, readback, 8 * 256 + 8 * 4, &center, sizeof(center)) ==
          GRANIT_SUCCESS);
  CHECK(corner == UINT32_C(0xff000000));
  CHECK(center == UINT32_C(0xff66b333));
  REQUIRE(loader.destroy_command_recorder(first, recorder) == GRANIT_SUCCESS);
  CHECK(loader.destroy_command_recorder(first, recorder) == GRANIT_ERROR_INVALID_HANDLE);

  REQUIRE(loader.create_command_recorder(first, &recorder) == GRANIT_SUCCESS);
  REQUIRE(loader.finish_command_recorder(first, recorder, &command_buffer) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_command_buffer(first, command_buffer) == GRANIT_SUCCESS);
  CHECK(loader.destroy_command_buffer(first, command_buffer) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.destroy_command_recorder(first, recorder) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_buffer(first, buffer) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_buffer(first, readback) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_texture_view(first, target_view) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_texture(first, target_texture) == GRANIT_SUCCESS);
  CHECK(loader.destroy_pipeline_layout(first, pipeline_layout) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_shader(first, vertex_shader) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_bind_group_layout(first, bind_group_layout) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_texture_view(first, view) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_sampler(first, sampler) == GRANIT_ERROR_INVALID_ARGUMENT);

  REQUIRE(loader.destroy_render_pipeline(first, pipeline) == GRANIT_SUCCESS);
  CHECK(loader.destroy_render_pipeline(first, pipeline) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.destroy_pipeline_layout(first, pipeline_layout) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_shader(first, vertex_shader) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_shader(first, fragment_shader) == GRANIT_SUCCESS);
  CHECK(loader.destroy_shader(first, fragment_shader) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.destroy_bind_group(first, bind_group) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_bind_group_layout(first, bind_group_layout) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_sampler(first, sampler) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_texture_view(first, view) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_texture(first, texture) == GRANIT_SUCCESS);

  REQUIRE(loader.create_bind_group_layout(first, &bind_group_layout) == GRANIT_SUCCESS);
  REQUIRE(loader.create_pipeline_layout(first, bind_group_layout, &pipeline_layout) ==
          GRANIT_SUCCESS);
  REQUIRE(loader.create_shader(first, &vertex_desc, &vertex_shader) == GRANIT_SUCCESS);
  REQUIRE(loader.create_shader(first, &fragment_desc, &fragment_shader) == GRANIT_SUCCESS);
  pipeline_desc.layout = pipeline_layout;
  pipeline_desc.vertex_shader = vertex_shader;
  pipeline_desc.fragment_shader = fragment_shader;
  REQUIRE(loader.create_render_pipeline(first, &pipeline_desc, &pipeline) == GRANIT_SUCCESS);
  REQUIRE(loader.create_command_recorder(first, &recorder) == GRANIT_SUCCESS);
  REQUIRE(loader.finish_command_recorder(first, recorder, &command_buffer) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(first) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(second) == GRANIT_SUCCESS);
  CHECK(state.allocations == state.deallocations);
}
