// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/render_target.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>

#include <catch2/catch_all.hpp>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>

namespace {

class wayland_test_window {
public:
  wayland_test_window() {
    display_ = wl_display_connect(nullptr);
    if (display_ == nullptr)
      return;
    registry_ = wl_display_get_registry(display_);
    if (registry_ == nullptr)
      return;
    wl_registry_add_listener(registry_, &registry_listener_, this);
    if (wl_display_roundtrip(display_) < 0 || compositor_ == nullptr || wm_base_ == nullptr)
      return;

    wl_surface_ = wl_compositor_create_surface(compositor_);
    if (wl_surface_ == nullptr)
      return;
    xdg_surface_ = xdg_wm_base_get_xdg_surface(wm_base_, wl_surface_);
    if (xdg_surface_ == nullptr)
      return;
    xdg_surface_add_listener(xdg_surface_, &surface_listener_, this);
    toplevel_ = xdg_surface_get_toplevel(xdg_surface_);
    if (toplevel_ == nullptr)
      return;
    xdg_toplevel_add_listener(toplevel_, &toplevel_listener_, this);
    xdg_toplevel_set_title(toplevel_, "Granit Wayland Swapchain Test");
    wl_surface_commit(wl_surface_);
    static_cast<void>(wl_display_roundtrip(display_));
  }

  ~wayland_test_window() {
    if (toplevel_ != nullptr)
      xdg_toplevel_destroy(toplevel_);
    if (xdg_surface_ != nullptr)
      xdg_surface_destroy(xdg_surface_);
    if (wl_surface_ != nullptr)
      wl_surface_destroy(wl_surface_);
    if (wm_base_ != nullptr)
      xdg_wm_base_destroy(wm_base_);
    if (compositor_ != nullptr)
      wl_compositor_destroy(compositor_);
    if (registry_ != nullptr)
      wl_registry_destroy(registry_);
    if (display_ != nullptr)
      wl_display_disconnect(display_);
  }

  wayland_test_window(const wayland_test_window&) = delete;
  wayland_test_window& operator=(const wayland_test_window&) = delete;

  [[nodiscard]] bool valid() const noexcept { return configured_; }
  [[nodiscard]] void* display() const noexcept { return display_; }
  [[nodiscard]] void* surface() const noexcept { return wl_surface_; }

private:
  static void registry_global(void* data, wl_registry* registry, std::uint32_t name,
                              const char* interface, std::uint32_t version) {
    auto& self = *static_cast<wayland_test_window*>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
      self.compositor_ = static_cast<wl_compositor*>(wl_registry_bind(
          registry, name, &wl_compositor_interface, std::min(version, UINT32_C(4))));
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
      self.wm_base_ = static_cast<xdg_wm_base*>(
          wl_registry_bind(registry, name, &xdg_wm_base_interface, UINT32_C(1)));
      if (self.wm_base_ != nullptr)
        xdg_wm_base_add_listener(self.wm_base_, &wm_base_listener_, &self);
    }
  }

  static void registry_remove(void*, wl_registry*, std::uint32_t) {}

  static void wm_base_ping(void*, xdg_wm_base* wm_base, std::uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
  }

  static void surface_configure(void* data, xdg_surface* surface, std::uint32_t serial) {
    auto& self = *static_cast<wayland_test_window*>(data);
    xdg_surface_ack_configure(surface, serial);
    self.configured_ = true;
  }

  static void toplevel_configure(void*, xdg_toplevel*, std::int32_t, std::int32_t, wl_array*) {}
  static void toplevel_close(void*, xdg_toplevel*) {}

  static constexpr wl_registry_listener registry_listener_{.global = registry_global,
                                                           .global_remove = registry_remove};
  static constexpr xdg_wm_base_listener wm_base_listener_{.ping = wm_base_ping};
  static constexpr xdg_surface_listener surface_listener_{.configure = surface_configure};
  inline static const xdg_toplevel_listener toplevel_listener_ = [] {
    xdg_toplevel_listener listener{};
    listener.configure = toplevel_configure;
    listener.close = toplevel_close;
    return listener;
  }();

  wl_display* display_{};
  wl_registry* registry_{};
  wl_compositor* compositor_{};
  xdg_wm_base* wm_base_{};
  wl_surface* wl_surface_{};
  xdg_surface* xdg_surface_{};
  xdg_toplevel* toplevel_{};
  bool configured_{};
};

bool environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable ||
         result == granit::result::incompatible_driver ||
         result == granit::result::no_suitable_device || result == granit::result::unsupported;
}

TEST_CASE("Wayland Surface 可以完成 Swapchain 清屏和 Present", "[swapchain][wayland]") {
  wayland_test_window window;
  if (!window.valid())
    SKIP("当前环境没有可用且支持 xdg-shell 的 Wayland compositor");

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize(
      {.application_name = "granit-wayland-tests", .surface_types = granit::surface_type::wayland});
  if (environment_unavailable(renderer_result))
    SKIP("当前环境不支持 Vulkan Wayland Swapchain");
  REQUIRE(renderer_result == granit::result::success);

  granit::surface surface;
  REQUIRE(surface.initialize_wayland(renderer.native_handle(),
                                     {.display = window.display(), .surface = window.surface()}) ==
          granit::result::success);
  granit::swapchain swapchain;
  REQUIRE(swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                               {.width = 96, .height = 72}) == granit::result::success);
  granit::swapchain_info info;
  REQUIRE(swapchain.query_info(info) == granit::result::success);

  granit::acquired_frame frame;
  REQUIRE(swapchain.acquire(frame) == granit::result::success);
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  REQUIRE(swapchain.backbuffer(frame.image_index, texture, view) == granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.08F, .green = 0.03F, .blue = 0.16F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {.width = info.width, .height = info.height}};
  REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
  REQUIRE(recorder.end_rendering() == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit(frame) == granit::result::success);
  REQUIRE(swapchain.present(frame) == granit::result::success);
  REQUIRE(wl_display_roundtrip(static_cast<wl_display*>(window.display())) >= 0);
  REQUIRE(recorder.reset() == granit::result::success);

  REQUIRE(swapchain.recreate({.width = 128, .height = 96}) == granit::result::success);
  REQUIRE(swapchain.query_info(info) == granit::result::success);
  granit::acquired_frame resized_frame;
  REQUIRE(swapchain.acquire(resized_frame) == granit::result::success);
  REQUIRE(swapchain.backbuffer(resized_frame.image_index, texture, view) ==
          granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit::color_attachment_desc resized_color{
      .view = view, .clear_value = {.red = 0.16F, .green = 0.06F, .blue = 0.03F, .alpha = 1.0F}};
  const granit::rendering_desc resized_rendering{
      .color_attachments = std::span{&resized_color, 1},
      .area = {.width = info.width, .height = info.height}};
  REQUIRE(recorder.begin_rendering(resized_rendering) == granit::result::success);
  REQUIRE(recorder.end_rendering() == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit(resized_frame) == granit::result::success);
  REQUIRE(swapchain.present(resized_frame) == granit::result::success);
  REQUIRE(wl_display_roundtrip(static_cast<wl_display*>(window.display())) >= 0);
}

} // namespace
