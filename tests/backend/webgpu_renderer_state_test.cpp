// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>

#include "backend/webgpu/renderer_state.h"

TEST_CASE("WebGPU Renderer 状态集中管理静态 Provider 生命周期", "[backend][webgpu][renderer]") {
  granit::detail::backend_plugin_loader module;
  REQUIRE(module.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);

  granit::detail::webgpu_renderer_state state;
  REQUIRE(state.initialize_static(module.api(), nullptr, nullptr) == GRANIT_SUCCESS);
  for (int attempt = 0; attempt < 2 && state.lifecycle_status().state ==
                                           granit::detail::backend_lifecycle_state::initializing;
       ++attempt) {
    REQUIRE(state.process_events() == GRANIT_SUCCESS);
  }

  REQUIRE(state.lifecycle_status().state == granit::detail::backend_lifecycle_state::ready);
  CHECK(state.lifecycle_status().failure_result == GRANIT_SUCCESS);
  CHECK(state.capabilities().uniform_buffer_offset_alignment == 256);
  CHECK(state.capabilities().storage_buffer_offset_alignment == 256);
  CHECK(state.capabilities().max_uniform_buffer_binding_size == 65536);
  CHECK(state.presentation() != nullptr);
  CHECK(state.initialize_static(module.api(), nullptr, nullptr) == GRANIT_ERROR_INVALID_ARGUMENT);
}
