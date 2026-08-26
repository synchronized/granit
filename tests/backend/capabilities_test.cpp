// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/capabilities.h"

#include <catch2/catch_all.hpp>

TEST_CASE("后端能力快照统一校验 Buffer 绑定限制", "[backend][capabilities]") {
  const granit::detail::backend_capabilities capabilities{
      .uniform_buffer_offset_alignment = 256,
      .storage_buffer_offset_alignment = 16,
      .max_uniform_buffer_binding_size = 65536,
      .max_storage_buffer_binding_size = 1024,
  };

  using granit::detail::backend_buffer_binding_type;
  CHECK(capabilities.supports_buffer_binding(backend_buffer_binding_type::uniform, 256, 65536));
  CHECK_FALSE(capabilities.supports_buffer_binding(backend_buffer_binding_type::uniform, 4, 16));
  CHECK_FALSE(capabilities.supports_buffer_binding(backend_buffer_binding_type::storage, 16, 2048));
  CHECK_FALSE(capabilities.supports_buffer_binding(backend_buffer_binding_type::storage, 16, 0));
}
