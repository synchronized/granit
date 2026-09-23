// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLE_APPLICATION_APPLICATION_H_
#define GRANIT_EXAMPLE_APPLICATION_APPLICATION_H_

#include "assets/asset_store.h"

#include <chrono>
#include <cstdint>
#include <string_view>

#include <granit/granit.hpp>
#include <granit/window.hpp>

namespace granit::example {

struct application_desc {
  std::string_view executable_path;
  std::string_view title{"Granit Example"};
  std::string_view application_name{"Granit Example"};
  std::uint32_t width{1280};
  std::uint32_t height{720};
  std::uint32_t smoke_test_frames{3};
  bool smoke_test{};
};

/** 一帧展示所需的借用资源；仅在 on_render 调用期间有效。 */
struct present_frame {
  acquired_frame& acquired;
  swapchain_backbuffer backbuffer;
  swapchain_info swapchain;
  float delta_seconds{};
};

/** 示例私有应用框架，统一窗口、Renderer 与 Swapchain 生命周期。 */
class application {
public:
  application() = default;
  virtual ~application() = default;

  application(const application&) = delete;
  application& operator=(const application&) = delete;

  [[nodiscard]] result run(const application_desc& desc = {}) noexcept;

  // run_window_loop 使用的公开协议；示例不应直接调用。
  [[nodiscard]] result tick(window_loop_action& action) noexcept;
  void shutdown(result reason) noexcept;

  [[nodiscard]] std::uint32_t rendered_frames() const noexcept { return rendered_frames_; }
  [[nodiscard]] std::uint32_t completed_recreates() const noexcept {
    return completed_recreates_;
  }
  [[nodiscard]] bool ready() const noexcept;

protected:
  [[nodiscard]] virtual result on_initialize() noexcept = 0;
  [[nodiscard]] virtual result on_render(present_frame& frame) noexcept = 0;
  [[nodiscard]] virtual result on_swapchain_changed(const swapchain_info& info) noexcept;
  [[nodiscard]] virtual result on_window_event(const window_event& event) noexcept;
  [[nodiscard]] virtual result on_input_event(const input_event& event) noexcept;
  virtual void on_shutdown(result reason) noexcept;

  [[nodiscard]] granit::renderer& renderer_owner() noexcept { return renderer_; }
  [[nodiscard]] renderer_ref renderer() const noexcept { return renderer_.ref(); }
  [[nodiscard]] granit::window& app_window() noexcept { return window_; }
  [[nodiscard]] assets::asset_store& assets() noexcept { return assets_; }
  [[nodiscard]] const swapchain_info& presentation_info() const noexcept {
    return swapchain_info_;
  }

  void request_stop() noexcept { running_ = false; }
  void request_swapchain_recreate() noexcept { recreate_ = true; }

private:
  enum class phase { fresh, renderer_initializing, running, stopped };

  [[nodiscard]] result poll_events() noexcept;
  [[nodiscard]] result initialize_presentation() noexcept;
  [[nodiscard]] result update_presentation(window_loop_action& action) noexcept;
  [[nodiscard]] result render_present_frame() noexcept;
  [[nodiscard]] bool smoke_complete() const noexcept;

  assets::asset_store assets_;
  window_system window_system_;
  granit::window window_;
  granit::renderer renderer_;
  surface surface_;
  swapchain swapchain_;
  window_state window_state_{};
  swapchain_info swapchain_info_{};
  application_desc desc_{};
  std::chrono::steady_clock::time_point previous_frame_time_{};
  phase phase_{phase::fresh};
  bool running_{true};
  bool recreate_{};
  bool content_started_{};
  std::uint32_t rendered_frames_{};
  std::uint32_t completed_recreates_{};
};

} // namespace granit::example

#endif
