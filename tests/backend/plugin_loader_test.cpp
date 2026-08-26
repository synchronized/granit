// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <new>
#include <stdexcept>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "backend/plugin_loader.h"

namespace {

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
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(state.allocations == 2);
  CHECK(state.deallocations == 1);
  state.throw_diagnostic = false;

  host.struct_size += 32;
  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);
  CHECK(instance != 0);
  CHECK(state.allocations == 3);
  CHECK(state.diagnostics == 3);

  CHECK(loader.destroy_instance(instance) == GRANIT_SUCCESS);
  CHECK(loader.destroy_instance(instance) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(state.deallocations == 2);
  CHECK(state.diagnostics == 4);

  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);

  loader.close();
  CHECK_FALSE(loader.is_open());
  CHECK(loader.api() == nullptr);
  CHECK(state.allocations == 4);
  CHECK(state.deallocations == 3);
  CHECK(state.diagnostics == 6);
  CHECK(loader.get_capabilities(instance, &capabilities) == GRANIT_ERROR_INVALID_ARGUMENT);
}
