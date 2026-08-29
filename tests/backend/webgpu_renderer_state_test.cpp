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
