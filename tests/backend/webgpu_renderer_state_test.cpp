// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

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
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT |
                       GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
  texture_desc.width = 64;
  texture_desc.height = 32;
  texture_desc.mip_levels = 4;
  REQUIRE(state.create_texture(texture_desc, *texture) == GRANIT_SUCCESS);

  auto texture_view = state.allocate_texture_view_resource();
  REQUIRE(texture_view != nullptr);
  granit_texture_view_desc view_desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
  view_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  view_desc.range.base_mip_level = 1;
  view_desc.range.mip_level_count = 2;
  REQUIRE(state.create_texture_view(*texture, texture_desc, view_desc, *texture_view) ==
          GRANIT_SUCCESS);

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
