// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/callback_lifetime.h"
#include "backend/capabilities.h"
#include "backend/lifecycle.h"
#include "renderer/dynamic_uniform_offsets.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <future>

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

TEST_CASE("动态 Uniform Offset 校验数量、对齐、范围和溢出", "[backend][capabilities]") {
  using granit::detail::dynamic_uniform_binding;
  using granit::detail::sort_dynamic_uniform_bindings;
  using granit::detail::validate_dynamic_uniform_offsets;

  std::array bindings{
      dynamic_uniform_binding{.binding = 5, .base_offset = 256, .range = 128, .buffer_size = 1024},
      dynamic_uniform_binding{.binding = 1, .base_offset = 0, .range = 64, .buffer_size = 1024},
  };
  sort_dynamic_uniform_bindings(bindings);
  REQUIRE(bindings[0].binding == 1);
  REQUIRE(bindings[1].binding == 5);
  const std::array valid_offsets{UINT32_C(256), UINT32_C(512)};
  CHECK(validate_dynamic_uniform_offsets(bindings, valid_offsets, 256));

  CHECK_FALSE(validate_dynamic_uniform_offsets(bindings, std::span{valid_offsets}.first(1), 256));
  const std::array unaligned_offsets{UINT32_C(4), UINT32_C(0)};
  CHECK_FALSE(validate_dynamic_uniform_offsets(bindings, unaligned_offsets, 256));
  const std::array out_of_bounds_offsets{UINT32_C(0), UINT32_C(768)};
  CHECK_FALSE(validate_dynamic_uniform_offsets(bindings, out_of_bounds_offsets, 256));

  const std::array overflow_binding{dynamic_uniform_binding{
      .binding = 0, .base_offset = UINT64_MAX - 8, .range = 16, .buffer_size = UINT64_MAX}};
  const std::array overflow_offset{UINT32_C(256)};
  CHECK_FALSE(validate_dynamic_uniform_offsets(overflow_binding, overflow_offset, 1));
}

TEST_CASE("后端生命周期统一门控初始化和终止状态", "[backend][lifecycle]") {
  using granit::detail::backend_lifecycle;
  using granit::detail::backend_lifecycle_state;

  backend_lifecycle lifecycle;
  CHECK(lifecycle.gate() == GRANIT_ERROR_NOT_READY);
  CHECK(lifecycle.status().state == backend_lifecycle_state::initializing);

  lifecycle.mark_ready();
  CHECK(lifecycle.gate() == GRANIT_SUCCESS);
  CHECK(lifecycle.status().state == backend_lifecycle_state::ready);

  lifecycle.mark_failed(GRANIT_ERROR_INITIALIZATION_FAILED);
  CHECK(lifecycle.gate() == GRANIT_ERROR_INITIALIZATION_FAILED);
  lifecycle.mark_ready();
  CHECK(lifecycle.status().state == backend_lifecycle_state::failed);

  lifecycle.mark_device_lost();
  CHECK(lifecycle.gate() == GRANIT_ERROR_DEVICE_LOST);
  CHECK(lifecycle.status().state == backend_lifecycle_state::device_lost);
}

TEST_CASE("后端异步回调票据在销毁后失效", "[backend][lifecycle][callback]") {
  granit::detail::backend_callback_lifetime lifetime;
  const auto ticket = lifetime.ticket();
  std::uint32_t calls{};
  CHECK(ticket.invoke([&calls] { ++calls; }));
  CHECK(calls == 1);

  lifetime.invalidate();
  CHECK_FALSE(ticket.invoke([&calls] { ++calls; }));
  CHECK(calls == 1);
}

TEST_CASE("后端销毁等待已经进入的异步回调", "[backend][lifecycle][callback]") {
  granit::detail::backend_callback_lifetime lifetime;
  const auto ticket = lifetime.ticket();
  std::promise<void> entered;
  std::promise<void> release;
  auto release_future = release.get_future();
  std::atomic_bool invalidated{};

  auto callback = std::async(std::launch::async, [&] {
    return ticket.invoke([&] {
      entered.set_value();
      release_future.wait();
    });
  });
  entered.get_future().wait();
  auto invalidation = std::async(std::launch::async, [&] {
    lifetime.invalidate();
    invalidated = true;
  });
  CHECK_FALSE(invalidated.load());
  release.set_value();
  CHECK(callback.get());
  invalidation.get();
  CHECK(invalidated.load());
  CHECK_FALSE(ticket.invoke([] {}));
}
