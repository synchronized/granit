// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <new>

#include <catch2/catch_all.hpp>

#include "backend/webgpu/presentation_adapter.h"

namespace {

void* allocate(std::uint64_t size, std::uint64_t alignment, void*) {
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void deallocate(void* memory, std::uint64_t size, std::uint64_t alignment, void* user_data);
void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category category,
              const char* message, std::uint32_t message_length, void* user_data);

TEST_CASE("WebGPU 呈现适配器转发桌面原生窗口参数", "[backend][webgpu][presentation]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);
  granit_backend_plugin_host_api host{sizeof(host), 0,          diagnose, nullptr,
                                      allocate,     deallocate, nullptr};
  granit_backend_plugin_instance instance{};
  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);
  REQUIRE(loader.process_events(instance) == GRANIT_SUCCESS);

  granit::detail::webgpu_presentation_adapter adapter{loader, instance};
  const auto native_a = reinterpret_cast<void*>(std::uintptr_t{1});
  const auto native_b = reinterpret_cast<void*>(std::uintptr_t{2});
  {
    auto surface = adapter.allocate_surface();
    REQUIRE(adapter.create_win32_surface(*surface, native_a, native_b) == GRANIT_SUCCESS);
  }
  {
    auto surface = adapter.allocate_surface();
    REQUIRE(adapter.create_xcb_surface(*surface, native_a, 42) == GRANIT_SUCCESS);
  }
  {
    auto surface = adapter.allocate_surface();
    REQUIRE(adapter.create_wayland_surface(*surface, native_a, native_b) == GRANIT_SUCCESS);
  }
  CHECK(loader.destroy_instance(instance) == GRANIT_SUCCESS);
}

void deallocate(void* memory, std::uint64_t, std::uint64_t alignment, void*) {
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void diagnose(granit_diagnostic_severity, granit_diagnostic_category, const char*, std::uint32_t,
              void*) {}

} // namespace

TEST_CASE("WebGPU 呈现适配器管理拥有资源和借用 Backbuffer", "[backend][webgpu][presentation]") {
  granit::detail::backend_plugin_loader loader;
  REQUIRE(loader.open(GRANIT_FAKE_BACKEND_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) ==
          GRANIT_SUCCESS);

  granit_backend_plugin_host_api host{sizeof(host), 0,          diagnose, nullptr,
                                      allocate,     deallocate, nullptr};
  granit_backend_plugin_instance instance{};
  REQUIRE(loader.create_instance(&host, &instance) == GRANIT_SUCCESS);
  REQUIRE(loader.process_events(instance) == GRANIT_SUCCESS);

  granit::detail::webgpu_presentation_adapter adapter{loader, instance};
  auto surface = adapter.allocate_surface();
  auto swapchain = adapter.allocate_swapchain();
  REQUIRE(surface != nullptr);
  REQUIRE(swapchain != nullptr);
  REQUIRE(adapter.create_canvas_surface(*surface, "#canvas", 7) == GRANIT_SUCCESS);

  granit::detail::backend_swapchain_desc desc{64, 48, 2,
                                              GRANIT_BACKEND_PLUGIN_PRESENT_MODE_MAILBOX};
  REQUIRE(adapter.create_swapchain(*surface, desc, *swapchain) == GRANIT_SUCCESS);

  granit::detail::backend_swapchain_info info{};
  REQUIRE(adapter.get_swapchain_info(*swapchain, info) == GRANIT_SUCCESS);
  CHECK(info.width == 64);
  CHECK(info.height == 48);
  CHECK(info.image_count == 1);
  CHECK(info.present_mode == GRANIT_BACKEND_PLUGIN_PRESENT_MODE_FIFO);
  CHECK(info.format == GRANIT_TEXTURE_FORMAT_RGBA8_UNORM);

  granit::detail::backend_acquired_swapchain_frame frame{};
  REQUIRE(adapter.acquire_swapchain(*swapchain, frame) == GRANIT_SUCCESS);
  CHECK(frame.image_index == 0);
  CHECK_FALSE(frame.needs_recreate);
  REQUIRE(frame.dynamic_backbuffer.texture != nullptr);
  REQUIRE(frame.dynamic_backbuffer.view != nullptr);
  CHECK(frame.dynamic_backbuffer.desc.width == 64);
  CHECK(frame.dynamic_backbuffer.desc.height == 48);
  CHECK(frame.dynamic_backbuffer.desc.format == GRANIT_TEXTURE_FORMAT_RGBA8_UNORM);
  CHECK(frame.dynamic_backbuffer.desc.usage == GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT);

  bool needs_recreate{};
  REQUIRE(adapter.present_swapchain(*swapchain, needs_recreate) == GRANIT_SUCCESS);
  CHECK_FALSE(needs_recreate);
  frame = {};

  REQUIRE(adapter.acquire_swapchain(*swapchain, frame) == GRANIT_SUCCESS);
  REQUIRE(adapter.cancel_swapchain(*swapchain, needs_recreate) == GRANIT_SUCCESS);
  CHECK_FALSE(needs_recreate);
  frame = {};

  desc.width = 80;
  desc.height = 60;
  REQUIRE(adapter.recreate_swapchain(*swapchain, desc) == GRANIT_SUCCESS);
  REQUIRE(adapter.get_swapchain_info(*swapchain, info) == GRANIT_SUCCESS);
  CHECK(info.width == 80);
  CHECK(info.height == 60);

  swapchain.reset();
  surface.reset();
  CHECK(loader.destroy_instance(instance) == GRANIT_SUCCESS);
}
