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
constexpr std::string_view compute_wgsl = R"(@compute @workgroup_size(1) fn cs_main() {})";

struct host_state {
  std::uint32_t allocations{};
  std::uint32_t deallocations{};
  std::uint32_t diagnostics{};
  bool fail_allocation{};
  bool throw_diagnostic{};
};

void deallocate(void* memory, uint64_t size, uint64_t alignment, void* user_data);
void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category category,
              const char* message, uint32_t message_length, void* user_data);
granit_backend_plugin_host_api make_host(host_state& state);

void* allocate(uint64_t size, uint64_t alignment, void* user_data) {
  auto& state = *static_cast<host_state*>(user_data);
  ++state.allocations;
  if (state.fail_allocation) {
    return nullptr;
  }
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

TEST_CASE("WebGPU 插件创建桌面原生 Surface", "[backend][plugin][surface]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  host_state state;
  auto host = make_host(state);
  granit_backend_plugin_instance instance{};
  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);
  REQUIRE(loader.process_events(instance) == GRANIT_SUCCESS);

  const auto native_a = reinterpret_cast<void*>(std::uintptr_t{1});
  const auto native_b = reinterpret_cast<void*>(std::uintptr_t{2});
  granit_backend_plugin_surface surface{};
  granit_backend_plugin_win32_surface_desc win32{sizeof(win32), 0, native_a, native_b};
  REQUIRE(loader.create_win32_surface(instance, &win32, &surface) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_surface(instance, surface) == GRANIT_SUCCESS);
  win32.window = nullptr;
  CHECK(loader.create_win32_surface(instance, &win32, &surface) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit_backend_plugin_xcb_surface_desc xcb{sizeof(xcb), 0, native_a, 42, 0};
  REQUIRE(loader.create_xcb_surface(instance, &xcb, &surface) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_surface(instance, surface) == GRANIT_SUCCESS);
  xcb.reserved_2 = 1;
  CHECK(loader.create_xcb_surface(instance, &xcb, &surface) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit_backend_plugin_wayland_surface_desc wayland{sizeof(wayland), 0, native_a, native_b};
  REQUIRE(loader.create_wayland_surface(instance, &wayland, &surface) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_surface(instance, surface) == GRANIT_SUCCESS);
  wayland.struct_size = 0;
  CHECK(loader.create_wayland_surface(instance, &wayland, &surface) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  REQUIRE(loader.destroy_instance(instance) == GRANIT_SUCCESS);
  CHECK(state.allocations == state.deallocations);
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
  CHECK(status.state == GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_INITIALIZING);
  CHECK(status.failure_result == GRANIT_SUCCESS);
  granit_backend_plugin_capabilities capabilities{};
  capabilities.struct_size = sizeof(capabilities);
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_NOT_READY);
  granit_backend_plugin_buffer_desc pending_buffer_desc{};
  pending_buffer_desc.struct_size = sizeof(pending_buffer_desc);
  pending_buffer_desc.size = 16;
  pending_buffer_desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  granit_backend_plugin_buffer pending_buffer{};
  CHECK(loader.create_buffer(instance, &pending_buffer_desc, &pending_buffer) ==
        GRANIT_ERROR_NOT_READY);
  CHECK(pending_buffer == 0);
  CHECK(loader.process_events(instance) == GRANIT_SUCCESS);
  CHECK(loader.get_instance_status(instance, &status) == GRANIT_SUCCESS);
  CHECK(status.state == GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_READY);

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
  capabilities = {};
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
  CHECK(loader.process_events(instance) == GRANIT_ERROR_DEVICE_LOST);
  status = {};
  status.struct_size = sizeof(status);
  CHECK(loader.get_instance_status(instance, &status) == GRANIT_SUCCESS);
  CHECK(status.state == GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_DEVICE_LOST);
  CHECK(status.failure_result == GRANIT_ERROR_DEVICE_LOST);
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_DEVICE_LOST);
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
  status = {};
  status.struct_size = sizeof(status);
  REQUIRE(loader.get_instance_status(instance, &status) == GRANIT_SUCCESS);
  CHECK(status.state == GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_INITIALIZING);
  REQUIRE(loader.process_events(instance) == GRANIT_ERROR_INITIALIZATION_FAILED);
  REQUIRE(loader.get_instance_status(instance, &status) == GRANIT_SUCCESS);
  CHECK(status.state == GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_FAILED);
  CHECK(status.failure_result == GRANIT_ERROR_INITIALIZATION_FAILED);
  capabilities = {};
  capabilities.struct_size = sizeof(capabilities);
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_INITIALIZATION_FAILED);
  CHECK(state.allocations == 3);
  CHECK(state.diagnostics == 8);

  CHECK(loader.destroy_instance(instance) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(instance) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(state.deallocations == 2);
  CHECK(state.diagnostics == 9);

  host.struct_size = sizeof(host);
  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);
  REQUIRE(loader.process_events(instance) == GRANIT_SUCCESS);

  loader.close();
  CHECK_FALSE(loader.is_open());
  CHECK(loader.api() == nullptr);
  CHECK(state.allocations == 4);
  CHECK(state.deallocations == 3);
  CHECK(state.diagnostics == 12);
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("后端插件 Loader 接入静态 Provider API", "[backend][plugin][static]") {
  granit::detail::backend_plugin_loader module;
  REQUIRE(module.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  REQUIRE(module.api() != nullptr);

  granit::detail::backend_plugin_loader loader;
  CHECK(loader.open_static(nullptr, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
        GRANIT_ERROR_INCOMPATIBLE_DRIVER);
  CHECK_FALSE(loader.is_open());
  REQUIRE(loader.open_static(module.api(), GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) == GRANIT_SUCCESS);
  CHECK(loader.is_open());
  CHECK(loader.api() == module.api());

  host_state state;
  auto host = make_host(state);
  granit_backend_plugin_instance instance{};
  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);
  REQUIRE(loader.process_events(instance) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(instance) == GRANIT_SUCCESS);
  loader.close();
  CHECK_FALSE(loader.is_open());
  CHECK(module.is_open());
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
  REQUIRE(loader.process_events(first) == GRANIT_SUCCESS);
  REQUIRE(loader.process_events(second) == GRANIT_SUCCESS);

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

TEST_CASE("WebGPU 插件 Canvas Surface 遵守生命周期与所有权契约", "[backend][plugin][surface]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  host_state state;
  auto host = make_host(state);
  granit_backend_plugin_instance instance{};
  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);

  granit_backend_plugin_canvas_surface_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.selector = "#canvas";
  desc.selector_length = 7;
  granit_backend_plugin_surface surface = 42;
  const auto initial_result = loader.create_canvas_surface(instance, &desc, &surface);
  const auto initial_surface = surface;
  REQUIRE((initial_result == GRANIT_ERROR_NOT_READY || initial_result == GRANIT_SUCCESS));
  if (initial_result == GRANIT_ERROR_NOT_READY) {
    CHECK(surface == 0);
    REQUIRE(loader.process_events(instance) == GRANIT_SUCCESS);
  } else {
    REQUIRE(surface != 0);
  }
  desc.struct_size = 0;
  surface = 42;
  CHECK(loader.create_canvas_surface(instance, &desc, &surface) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(surface == 0);
  desc.struct_size = sizeof(desc);
  CHECK(loader.create_canvas_surface(instance + 1, &desc, &surface) == GRANIT_ERROR_INVALID_HANDLE);
  if (initial_result == GRANIT_ERROR_NOT_READY) {
    REQUIRE(loader.create_canvas_surface(instance, &desc, &surface) == GRANIT_SUCCESS);
    REQUIRE(surface != 0);
  } else {
    surface = initial_surface;
  }

  granit_backend_plugin_swapchain_desc swapchain_desc{};
  swapchain_desc.struct_size = sizeof(swapchain_desc);
  swapchain_desc.width = 640;
  swapchain_desc.height = 480;
  swapchain_desc.minimum_image_count = 2;
  swapchain_desc.present_mode = GRANIT_BACKEND_PLUGIN_PRESENT_MODE_MAILBOX;
  granit_backend_plugin_swapchain swapchain = 42;
  REQUIRE(loader.create_swapchain(instance, surface, &swapchain_desc, &swapchain) ==
          GRANIT_SUCCESS);
  REQUIRE(swapchain != 0);
  CHECK(loader.destroy_surface(instance, surface) == GRANIT_ERROR_INVALID_ARGUMENT);
  granit_backend_plugin_swapchain_info info{};
  info.struct_size = sizeof(info);
  REQUIRE(loader.get_swapchain_info(instance, swapchain, &info) == GRANIT_SUCCESS);
  CHECK(info.width == 640);
  CHECK(info.height == 480);
  CHECK(info.image_count == 1);
  CHECK(info.present_mode == GRANIT_BACKEND_PLUGIN_PRESENT_MODE_FIFO);
  CHECK(info.format == GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM);
  granit_backend_plugin_acquired_frame frame{};
  frame.struct_size = sizeof(frame);
  REQUIRE(loader.acquire_swapchain(instance, swapchain, &frame) == GRANIT_SUCCESS);
  REQUIRE(frame.texture != 0);
  REQUIRE(frame.view != 0);
  granit_backend_plugin_acquired_frame duplicate{};
  duplicate.struct_size = sizeof(duplicate);
  CHECK(loader.acquire_swapchain(instance, swapchain, &duplicate) == GRANIT_ERROR_NOT_READY);
  CHECK(loader.destroy_texture(instance, frame.texture) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_texture_view(instance, frame.view) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.recreate_swapchain(instance, swapchain, &swapchain_desc) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  std::uint32_t needs_recreate{};
  REQUIRE(loader.present_swapchain(instance, swapchain, &needs_recreate) == GRANIT_SUCCESS);
  CHECK(needs_recreate == 0);
  CHECK(loader.destroy_texture(instance, frame.texture) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(loader.destroy_texture_view(instance, frame.view) == GRANIT_ERROR_INVALID_HANDLE);

  frame = {};
  frame.struct_size = sizeof(frame);
  REQUIRE(loader.acquire_swapchain(instance, swapchain, &frame) == GRANIT_SUCCESS);
  REQUIRE(loader.cancel_swapchain(instance, swapchain, &needs_recreate) == GRANIT_SUCCESS);
  swapchain_desc.width = 800;
  swapchain_desc.height = 600;
  REQUIRE(loader.recreate_swapchain(instance, swapchain, &swapchain_desc) == GRANIT_SUCCESS);
  info = {};
  info.struct_size = sizeof(info);
  REQUIRE(loader.get_swapchain_info(instance, swapchain, &info) == GRANIT_SUCCESS);
  CHECK(info.width == 800);
  CHECK(info.height == 600);

  REQUIRE(loader.destroy_swapchain(instance, swapchain) == GRANIT_SUCCESS);
  CHECK(loader.destroy_swapchain(instance, swapchain) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.destroy_surface(instance, surface) == GRANIT_SUCCESS);
  CHECK(loader.destroy_surface(instance, surface) == GRANIT_ERROR_INVALID_HANDLE);

  REQUIRE(loader.create_canvas_surface(instance, &desc, &surface) == GRANIT_SUCCESS);
  swapchain_desc.width = 64;
  swapchain_desc.height = 64;
  REQUIRE(loader.create_swapchain(instance, surface, &swapchain_desc, &swapchain) ==
          GRANIT_SUCCESS);
  frame = {};
  frame.struct_size = sizeof(frame);
  REQUIRE(loader.acquire_swapchain(instance, swapchain, &frame) == GRANIT_SUCCESS);

  CHECK(loader.destroy_instance(instance) == GRANIT_SUCCESS);
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
  REQUIRE(loader.process_events(first) == GRANIT_SUCCESS);
  REQUIRE(loader.process_events(second) == GRANIT_SUCCESS);

  granit_backend_plugin_texture_desc texture_desc{};
  texture_desc.struct_size = sizeof(texture_desc);
  texture_desc.width = 64;
  texture_desc.height = 32;
  texture_desc.usage = GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT |
                       GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT;
  texture_desc.format = GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.mip_level_count = 1;
  granit_backend_plugin_texture texture{};
  REQUIRE(loader.create_texture(first, &texture_desc, &texture) == GRANIT_SUCCESS);
  REQUIRE(texture != 0);
  const granit_backend_plugin_texture_write_desc write_desc{
      sizeof(granit_backend_plugin_texture_write_desc), 0, 2, 3, 3, 2, 16, 2, {0, 0}};
  const std::array<std::uint8_t, 32> pixels{};
  REQUIRE(loader.write_texture(first, texture, &write_desc, pixels.data(), pixels.size()) ==
          GRANIT_SUCCESS);
  auto invalid_write = write_desc;
  invalid_write.mip_level = 1;
  CHECK(loader.write_texture(first, texture, &invalid_write, pixels.data(), pixels.size()) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  granit_backend_plugin_texture_view view{};
  const granit_backend_plugin_texture_view_desc view_desc{
      sizeof(granit_backend_plugin_texture_view_desc),
      GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM,
      0,
      1,
      {0, 0}};
  REQUIRE(loader.create_texture_view(first, texture, &view_desc, &view) == GRANIT_SUCCESS);
  REQUIRE(view != 0);
  granit_backend_plugin_texture_view foreign_view = 123;
  CHECK(loader.create_texture_view(second, texture, &view_desc, &foreign_view) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(foreign_view == 0);
  CHECK(loader.destroy_texture(first, texture) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_texture_view(second, view) == GRANIT_ERROR_INVALID_HANDLE);

  granit_backend_plugin_sampler_desc sampler_desc{};
  sampler_desc.struct_size = sizeof(sampler_desc);
  sampler_desc.min_filter = GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  sampler_desc.mag_filter = GRANIT_BACKEND_PLUGIN_FILTER_NEAREST;
  sampler_desc.mipmap_filter = GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  sampler_desc.address_mode_u = GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_REPEAT;
  sampler_desc.address_mode_v = GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_MIRROR_REPEAT;
  sampler_desc.address_mode_w = GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_desc.compare_operation = GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_LESS_EQUAL;
  sampler_desc.max_anisotropy = 1;
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
  REQUIRE(loader.create_texture_view(first, texture, &view_desc, &view) == GRANIT_SUCCESS);
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
  REQUIRE(loader.process_events(first) == GRANIT_SUCCESS);
  REQUIRE(loader.process_events(second) == GRANIT_SUCCESS);

  granit_backend_plugin_texture_desc texture_desc{};
  texture_desc.struct_size = sizeof(texture_desc);
  texture_desc.width = 16;
  texture_desc.height = 16;
  texture_desc.usage = GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT |
                       GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT;
  texture_desc.format = GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.mip_level_count = 1;
  granit_backend_plugin_texture texture{};
  granit_backend_plugin_texture_view view{};
  const granit_backend_plugin_texture_view_desc view_desc{
      sizeof(granit_backend_plugin_texture_view_desc),
      GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM,
      0,
      1,
      {0, 0}};
  REQUIRE(loader.create_texture(first, &texture_desc, &texture) == GRANIT_SUCCESS);
  REQUIRE(loader.create_texture_view(first, texture, &view_desc, &view) == GRANIT_SUCCESS);
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
  REQUIRE(loader.create_sampler(first, &sampler_desc, &sampler) == GRANIT_SUCCESS);

  granit_backend_plugin_buffer_desc uniform_desc{};
  uniform_desc.struct_size = sizeof(uniform_desc);
  uniform_desc.size = 1024;
  uniform_desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_UNIFORM_BIT |
                       GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  granit_backend_plugin_buffer uniform_buffer{};
  REQUIRE(loader.create_buffer(first, &uniform_desc, &uniform_buffer) == GRANIT_SUCCESS);
  const std::array<std::byte, 16> batch_data{};
  std::array<granit_backend_plugin_upload_operation, 2> upload_operations{};
  for (std::size_t index = 0; index < upload_operations.size(); ++index) {
    auto& operation = upload_operations[index];
    operation.struct_size = sizeof(operation);
    operation.type = GRANIT_BACKEND_PLUGIN_UPLOAD_TYPE_BUFFER;
    operation.buffer = uniform_buffer;
    operation.destination_offset = index * batch_data.size();
    operation.data = batch_data.data();
    operation.size = batch_data.size();
  }
  REQUIRE(loader.write_upload_batch(first, upload_operations) == GRANIT_SUCCESS);
  upload_operations[1].destination_offset = 1;
  CHECK(loader.write_upload_batch(first, upload_operations) == GRANIT_ERROR_INVALID_ARGUMENT);
  upload_operations[1].destination_offset = batch_data.size();

  const granit_backend_plugin_bind_group_layout_entry layout_entries[]{
      {0, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE,
       GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT, 1},
      {1, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER, GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT,
       1},
      {2, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER,
       GRANIT_BACKEND_PLUGIN_SHADER_STAGE_VERTEX, 1}};
  const granit_backend_plugin_bind_group_layout_desc layout_desc{
      sizeof(granit_backend_plugin_bind_group_layout_desc), 3, layout_entries, 0};
  granit_backend_plugin_bind_group_layout bind_group_layout{};
  REQUIRE(loader.create_bind_group_layout(first, &layout_desc, &bind_group_layout) ==
          GRANIT_SUCCESS);
  const granit_backend_plugin_bind_group_entry group_entries[]{
      {0, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE, 0, view, 0, 0, 0},
      {1, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER, 0, 0, sampler, 0, 0},
      {2, GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER, uniform_buffer, 0, 0, 0, 256}};
  granit_backend_plugin_bind_group_desc bind_group_desc{};
  bind_group_desc.struct_size = sizeof(bind_group_desc);
  bind_group_desc.entry_count = 3;
  bind_group_desc.layout = bind_group_layout;
  bind_group_desc.entries = group_entries;
  granit_backend_plugin_bind_group bind_group{};
  REQUIRE(loader.create_bind_group(first, &bind_group_desc, &bind_group) == GRANIT_SUCCESS);
  granit_backend_plugin_bind_group foreign_bind_group = 123;
  CHECK(loader.create_bind_group(second, &bind_group_desc, &foreign_bind_group) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(foreign_bind_group == 0);

  const granit_backend_plugin_bind_group_layout pipeline_layouts[]{bind_group_layout,
                                                                   bind_group_layout};
  const granit_backend_plugin_pipeline_layout_desc pipeline_layout_desc{
      sizeof(granit_backend_plugin_pipeline_layout_desc), 2, pipeline_layouts, 0};
  granit_backend_plugin_pipeline_layout pipeline_layout{};
  REQUIRE(loader.create_pipeline_layout(first, &pipeline_layout_desc, &pipeline_layout) ==
          GRANIT_SUCCESS);
  granit_backend_plugin_pipeline_layout foreign_layout = 123;
  CHECK(loader.create_pipeline_layout(second, &pipeline_layout_desc, &foreign_layout) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(foreign_layout == 0);
  const granit_backend_plugin_pipeline_layout_desc empty_pipeline_layout_desc{
      sizeof(granit_backend_plugin_pipeline_layout_desc), 0, nullptr, 0};
  granit_backend_plugin_pipeline_layout empty_pipeline_layout{};
  REQUIRE(loader.create_pipeline_layout(first, &empty_pipeline_layout_desc,
                                        &empty_pipeline_layout) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_pipeline_layout(first, empty_pipeline_layout) == GRANIT_SUCCESS);
  const granit_backend_plugin_pipeline_layout_desc invalid_pipeline_layout_desc{
      sizeof(granit_backend_plugin_pipeline_layout_desc), 1, nullptr, 0};
  CHECK(loader.create_pipeline_layout(first, &invalid_pipeline_layout_desc, &foreign_layout) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(foreign_layout == 0);
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
  auto compute_desc = vertex_desc;
  compute_desc.stage = GRANIT_BACKEND_PLUGIN_SHADER_STAGE_COMPUTE;
  compute_desc.wgsl = compute_wgsl.data();
  compute_desc.wgsl_length = compute_wgsl.size();
  compute_desc.entry_point = "cs_main";
  granit_backend_plugin_shader compute_shader{};
  REQUIRE(loader.create_shader(first, &compute_desc, &compute_shader) == GRANIT_SUCCESS);
  granit_backend_plugin_compute_pipeline_desc compute_pipeline_desc{
      sizeof(granit_backend_plugin_compute_pipeline_desc), 0, pipeline_layout, compute_shader};
  granit_backend_plugin_compute_pipeline compute_pipeline{};
  REQUIRE(loader.create_compute_pipeline(first, &compute_pipeline_desc, &compute_pipeline) ==
          GRANIT_SUCCESS);
  compute_pipeline_desc.shader = vertex_shader;
  granit_backend_plugin_compute_pipeline invalid_compute = 123;
  CHECK(loader.create_compute_pipeline(first, &compute_pipeline_desc, &invalid_compute) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(invalid_compute == 0);
  compute_pipeline_desc.shader = compute_shader;
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
      GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_ALWAYS};
  granit_backend_plugin_render_pipeline pipeline{};
  REQUIRE(loader.create_render_pipeline(first, &pipeline_desc, &pipeline) == GRANIT_SUCCESS);
  const granit_backend_plugin_vertex_attribute vertex_attributes[]{
      {0, GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X3, 0, 0},
      {1, GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X2, 12, 0}};
  const granit_backend_plugin_vertex_attribute instance_attributes[]{
      {2, GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X4, 0, 0},
      {3, GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X4, 16, 0},
      {4, GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X4, 32, 0}};
  const granit_backend_plugin_vertex_buffer_layout vertex_layouts[]{
      {20, GRANIT_BACKEND_PLUGIN_VERTEX_STEP_MODE_VERTEX, 2, 0, vertex_attributes},
      {48, GRANIT_BACKEND_PLUGIN_VERTEX_STEP_MODE_INSTANCE, 3, 0, instance_attributes}};
  pipeline_desc.vertex_buffer_layout_count = 2;
  pipeline_desc.vertex_buffer_layouts = vertex_layouts;
  granit_backend_plugin_render_pipeline geometry_pipeline{};
  REQUIRE(loader.create_render_pipeline(first, &pipeline_desc, &geometry_pipeline) ==
          GRANIT_SUCCESS);
  auto invalid_layout = vertex_layouts[0];
  invalid_layout.stride = 16;
  pipeline_desc.vertex_buffer_layout_count = 1;
  pipeline_desc.vertex_buffer_layouts = &invalid_layout;
  granit_backend_plugin_render_pipeline invalid_pipeline = 42;
  CHECK(loader.create_render_pipeline(first, &pipeline_desc, &invalid_pipeline) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(invalid_pipeline == 0);
  pipeline_desc.vertex_buffer_layout_count = 0;
  pipeline_desc.vertex_buffer_layouts = nullptr;
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
  REQUIRE(loader.create_texture_view(first, target_texture, &view_desc, &target_view) ==
          GRANIT_SUCCESS);

  granit_backend_plugin_buffer_desc buffer_desc{};
  buffer_desc.struct_size = sizeof(buffer_desc);
  buffer_desc.size = 4096;
  buffer_desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT |
                      GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_VERTEX_BIT |
                      GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_INDEX_BIT;
  granit_backend_plugin_buffer buffer{};
  REQUIRE(loader.create_buffer(first, &buffer_desc, &buffer) == GRANIT_SUCCESS);
  granit_backend_plugin_command_recorder recorder{};
  REQUIRE(loader.create_command_recorder(first, &recorder) == GRANIT_SUCCESS);
  CHECK(loader.recorder_copy_buffer_to_texture(first, recorder, buffer, texture, 16, 16, 64) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(loader.recorder_copy_buffer_to_texture(first, recorder, buffer, texture, 16, 16, 256) ==
          GRANIT_SUCCESS);
  const granit_backend_plugin_vertex_buffer_binding vertex_binding{buffer, 0};
  const float clear_color[]{0.0F, 0.0F, 0.0F, 1.0F};
  REQUIRE(loader.recorder_begin_rendering(
              first, recorder, target_view, GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR,
              GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE, clear_color) == GRANIT_SUCCESS);
  const granit_backend_plugin_viewport viewport{0.0F, 0.0F, 16.0F, 16.0F, 0.0F, 1.0F};
  const granit_backend_plugin_scissor scissor{0, 0, 16, 16};
  REQUIRE(loader.recorder_set_viewports(first, recorder, 0, std::span{&viewport, 1}) ==
          GRANIT_SUCCESS);
  REQUIRE(loader.recorder_set_scissors(first, recorder, 0, std::span{&scissor, 1}) ==
          GRANIT_SUCCESS);
  CHECK(loader.recorder_set_viewports(first, recorder, 1, std::span{&viewport, 1}) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.recorder_set_scissors(first, recorder, 1, std::span{&scissor, 1}) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  const granit_backend_plugin_bind_group graphics_groups[]{bind_group};
  const std::uint32_t dynamic_offset[]{256};
  REQUIRE(loader.recorder_bind_graphics_groups(first, recorder, pipeline_layout, 1, graphics_groups,
                                               dynamic_offset) == GRANIT_SUCCESS);
  const std::uint32_t unexpected_dynamic_offset[]{1};
  CHECK(loader.recorder_bind_graphics_groups(first, recorder, pipeline_layout, 0, graphics_groups,
                                             unexpected_dynamic_offset) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.recorder_bind_graphics_groups(first, recorder, pipeline_layout, 2, graphics_groups,
                                             {}) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.recorder_draw_vertices(first, recorder, 3, 1, 0, 0) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(loader.recorder_bind_pipeline(first, recorder, pipeline) == GRANIT_SUCCESS);
  REQUIRE(loader.recorder_bind_vertex_buffers(first, recorder, 0, std::span{&vertex_binding, 1}) ==
          GRANIT_SUCCESS);
  REQUIRE(loader.recorder_draw_vertices(first, recorder, 3, 2, 1, 4) == GRANIT_SUCCESS);
  REQUIRE(loader.recorder_bind_index_buffer(first, recorder, buffer, 0,
                                            GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT16) ==
          GRANIT_SUCCESS);
  CHECK(loader.recorder_draw_indices(first, recorder, 3000, 1, 0, 0, 0) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(loader.recorder_draw_indices(first, recorder, 3, 1, 0, -1, 0) == GRANIT_SUCCESS);
  REQUIRE(loader.recorder_end_rendering(first, recorder) == GRANIT_SUCCESS);
  CHECK(loader.recorder_end_rendering(first, recorder) == GRANIT_ERROR_INVALID_ARGUMENT);
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
  CHECK(loader.recorder_draw_vertices(first, recorder, 3, 1, 0, 0) ==
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
  REQUIRE(loader.create_command_recorder(first, &recorder) == GRANIT_SUCCESS);
  REQUIRE(loader.recorder_begin_compute(first, recorder) == GRANIT_SUCCESS);
  CHECK(loader.recorder_dispatch(first, recorder, 1, 1, 1) == GRANIT_ERROR_INVALID_ARGUMENT);
  REQUIRE(loader.recorder_bind_compute_pipeline(first, recorder, compute_pipeline) ==
          GRANIT_SUCCESS);
  const std::uint32_t compute_dynamic_offset[]{256};
  REQUIRE(loader.recorder_bind_compute_groups(first, recorder, pipeline_layout, 0, graphics_groups,
                                              compute_dynamic_offset) == GRANIT_SUCCESS);
  REQUIRE(loader.recorder_dispatch(first, recorder, 2, 1, 1) == GRANIT_SUCCESS);
  REQUIRE(loader.recorder_end_compute(first, recorder) == GRANIT_SUCCESS);
  REQUIRE(loader.finish_command_recorder(first, recorder, &command_buffer) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_command_buffer(first, command_buffer) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_command_recorder(first, recorder) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_buffer(first, buffer) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_buffer(first, readback) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_texture_view(first, target_view) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_texture(first, target_texture) == GRANIT_SUCCESS);
  CHECK(loader.destroy_pipeline_layout(first, pipeline_layout) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_shader(first, compute_shader) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_shader(first, vertex_shader) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_bind_group_layout(first, bind_group_layout) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_texture_view(first, view) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(loader.destroy_sampler(first, sampler) == GRANIT_ERROR_INVALID_ARGUMENT);

  REQUIRE(loader.destroy_render_pipeline(first, pipeline) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_render_pipeline(first, geometry_pipeline) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_compute_pipeline(first, compute_pipeline) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_shader(first, compute_shader) == GRANIT_SUCCESS);
  CHECK(loader.destroy_render_pipeline(first, pipeline) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.destroy_pipeline_layout(first, pipeline_layout) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_shader(first, vertex_shader) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_shader(first, fragment_shader) == GRANIT_SUCCESS);
  CHECK(loader.destroy_shader(first, fragment_shader) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(loader.destroy_bind_group(first, bind_group) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_bind_group_layout(first, bind_group_layout) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_buffer(first, uniform_buffer) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_sampler(first, sampler) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_texture_view(first, view) == GRANIT_SUCCESS);
  REQUIRE(loader.destroy_texture(first, texture) == GRANIT_SUCCESS);

  REQUIRE(loader.create_bind_group_layout(first, &layout_desc, &bind_group_layout) ==
          GRANIT_SUCCESS);
  const granit_backend_plugin_bind_group_layout recreated_layouts[]{bind_group_layout};
  const granit_backend_plugin_pipeline_layout_desc recreated_pipeline_layout_desc{
      sizeof(granit_backend_plugin_pipeline_layout_desc), 1, recreated_layouts, 0};
  REQUIRE(loader.create_pipeline_layout(first, &recreated_pipeline_layout_desc, &pipeline_layout) ==
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
