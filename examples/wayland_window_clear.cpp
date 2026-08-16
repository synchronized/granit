// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>

namespace {

class wayland_window {
public:
  wayland_window() {
    display_ = wl_display_connect(nullptr);
    if (display_ == nullptr)
      return;
    registry_ = wl_display_get_registry(display_);
    if (registry_ == nullptr)
      return;
    wl_registry_add_listener(registry_, &registry_listener_, this);
    if (wl_display_roundtrip(display_) < 0 || compositor_ == nullptr || wm_base_ == nullptr)
      return;

    surface_ = wl_compositor_create_surface(compositor_);
    if (surface_ == nullptr)
      return;
    xdg_surface_ = xdg_wm_base_get_xdg_surface(wm_base_, surface_);
    if (xdg_surface_ == nullptr)
      return;
    xdg_surface_add_listener(xdg_surface_, &surface_listener_, this);
    toplevel_ = xdg_surface_get_toplevel(xdg_surface_);
    if (toplevel_ == nullptr)
      return;
    xdg_toplevel_add_listener(toplevel_, &toplevel_listener_, this);
    xdg_toplevel_set_title(toplevel_, "Granit Wayland 窗口清屏");
    wl_surface_commit(surface_);
    static_cast<void>(wl_display_roundtrip(display_));
  }

  ~wayland_window() {
    if (toplevel_ != nullptr)
      xdg_toplevel_destroy(toplevel_);
    if (xdg_surface_ != nullptr)
      xdg_surface_destroy(xdg_surface_);
    if (surface_ != nullptr)
      wl_surface_destroy(surface_);
    if (wm_base_ != nullptr)
      xdg_wm_base_destroy(wm_base_);
    if (compositor_ != nullptr)
      wl_compositor_destroy(compositor_);
    if (registry_ != nullptr)
      wl_registry_destroy(registry_);
    if (display_ != nullptr)
      wl_display_disconnect(display_);
  }

  wayland_window(const wayland_window&) = delete;
  wayland_window& operator=(const wayland_window&) = delete;

  [[nodiscard]] bool valid() const noexcept { return configured_; }
  [[nodiscard]] bool closed() const noexcept { return closed_; }
  [[nodiscard]] bool resized() const noexcept { return resized_; }
  [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
  [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
  [[nodiscard]] void* display() const noexcept { return display_; }
  [[nodiscard]] void* surface() const noexcept { return surface_; }

  void acknowledge_resize() noexcept { resized_ = false; }
  [[nodiscard]] bool dispatch() noexcept { return wl_display_roundtrip(display_) >= 0; }

private:
  static void registry_global(void* data, wl_registry* registry, std::uint32_t name,
                              const char* interface, std::uint32_t version) {
    auto& self = *static_cast<wayland_window*>(data);
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
    auto& self = *static_cast<wayland_window*>(data);
    xdg_surface_ack_configure(surface, serial);
    self.configured_ = true;
  }
  static void toplevel_configure(void* data, xdg_toplevel*, std::int32_t width, std::int32_t height,
                                 wl_array*) {
    if (width <= 0 || height <= 0)
      return;
    auto& self = *static_cast<wayland_window*>(data);
    const auto next_width = static_cast<std::uint32_t>(width);
    const auto next_height = static_cast<std::uint32_t>(height);
    self.resized_ = self.resized_ || next_width != self.width_ || next_height != self.height_;
    self.width_ = next_width;
    self.height_ = next_height;
  }
  static void toplevel_close(void* data, xdg_toplevel*) {
    static_cast<wayland_window*>(data)->closed_ = true;
  }

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
  wl_surface* surface_{};
  xdg_surface* xdg_surface_{};
  xdg_toplevel* toplevel_{};
  std::uint32_t width_{800};
  std::uint32_t height_{600};
  bool configured_{};
  bool closed_{};
  bool resized_{};
};

granit::result render_frame(granit::swapchain& swapchain, granit::command_recorder& recorder,
                            std::uint32_t width, std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (granit::failed(result))
    return result;
  needs_recreate = frame.needs_recreate;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  result = swapchain.backbuffer(frame.image_index, texture, view);
  if (granit::succeeded(result))
    result = recorder.begin();
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.05F, .green = 0.14F, .blue = 0.10F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, width, height}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(rendering);
  if (granit::succeeded(result))
    result = recorder.end_rendering();
  if (granit::succeeded(result))
    result = recorder.end();
  if (granit::succeeded(result))
    result = recorder.submit(frame);
  if (granit::succeeded(result))
    result = swapchain.present(frame);
  needs_recreate = needs_recreate || frame.needs_recreate;
  const auto reset_result = recorder.reset();
  return granit::failed(result) ? result : reset_result;
}

} // namespace

int main(int argc, char** argv) {
  const bool smoke_test = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
  wayland_window window;
  if (!window.valid())
    return 1;

  granit::renderer renderer;
  auto result = renderer.initialize({.application_name = "Granit Wayland Window Clear",
                                     .enable_validation = true,
                                     .surface_types = granit::surface_type::wayland});
  granit::surface surface;
  if (granit::succeeded(result)) {
    result = surface.initialize_wayland(renderer.native_handle(),
                                        {.display = window.display(), .surface = window.surface()});
  }
  granit::swapchain swapchain;
  if (granit::succeeded(result)) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = window.width(), .height = window.height()});
  }
  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());

  bool recreate = false;
  std::uint32_t rendered_frames = 0;
  while (granit::succeeded(result) && !window.closed()) {
    if (!window.dispatch()) {
      result = granit::result::backend_unavailable;
      break;
    }
    recreate = recreate || window.resized();
    if (recreate) {
      result = swapchain.recreate({.width = window.width(), .height = window.height()});
      if (result == granit::result::not_ready)
        continue;
      if (granit::failed(result))
        break;
      window.acknowledge_resize();
      recreate = false;
    }
    result = render_frame(swapchain, recorder, window.width(), window.height(), recreate);
    if (result == granit::result::out_of_date) {
      recreate = true;
      result = granit::result::success;
      continue;
    }
    if (granit::succeeded(result) && smoke_test && ++rendered_frames >= 3)
      break;
  }

  recorder.reset();
  swapchain.reset();
  surface.reset();
  renderer.reset();
  return granit::failed(result) ? 1 : 0;
}
