// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/frame_context.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/window.hpp>

#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << "\n";
  return 1;
}

granit::result poll_window_events(granit::window_system& window_system, bool& running,
                                  bool& recreate) {
  granit::window_event event;
  granit::result result;
  while ((result = window_system.poll(event)).ok()) {
    if (event.type == granit::window_event_type::close_requested)
      running = false;
    if (event.type == granit::window_event_type::resized ||
        event.type == granit::window_event_type::scale_changed ||
        event.type == granit::window_event_type::native_handle_changed) {
      recreate = true;
    }
  }
  return result == granit::result::not_ready ? granit::result::success : result;
}

granit::result poll_input_events(granit::window_system& window_system, bool& running) {
  granit::input_event event;
  granit::result result;
  while ((result = window_system.poll(event)).ok()) {
    if (event.type == granit::input_event_type::key &&
        event.data.key.action == granit::key_action::released &&
        event.data.key.physical == granit::physical_key::escape) {
      running = false;
    }
  }
  return result == granit::result::not_ready ? granit::result::success : result;
}

granit::result render_clear_frame(granit::swapchain& swapchain, granit::frame_context& context,
                                  const granit::swapchain_info& info, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (result.failed())
    return result;
  needs_recreate = needs_recreate || frame.needs_recreate();

  granit::swapchain_backbuffer backbuffer;
  result = swapchain.backbuffer(frame, backbuffer);

  granit::frame_recording recording;
  if (result.ok())
    result = context.begin(frame, recording);

  const granit::color_attachment_desc color{
      .view = backbuffer.view,
      .resolve_view = {},
      .clear_value = {.red = 0.04F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, info.width, info.height}};
  if (result.ok())
    result = recording.recorder().begin_rendering(rendering);
  if (result.ok())
    result = recording.recorder().end_rendering();
  if (result.ok())
    result = recording.submit();
  if (result.ok())
    result = swapchain.present(frame);

  needs_recreate = needs_recreate || frame.needs_recreate();
  if (result.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(swapchain.cancel(frame));
  }
  return result;
}

enum class application_phase { renderer_initializing, running, stopped };

class tutorial_application {
public:
  granit::result initialize(bool smoke_test) noexcept {
    smoke_test_ = smoke_test;
    auto result = window_system_.initialize();
    if (result.failed())
      return result;
    result = window_.initialize(window_system_,
                                {.title = "Granit Tutorial 01", .width = 1280, .height = 720});
    if (result.failed())
      return result;
    return renderer_.initialize({.application_name = "Granit Tutorial 01",
                                 .presentation = granit::presentation_mode::enabled});
  }

  granit::result tick(granit::window_loop_action& action) noexcept {
    auto result = poll_window_events(window_system_, running_, recreate_);
    if (result.ok())
      result = poll_input_events(window_system_, running_);
    if (result.failed())
      return result;
    if (!running_) {
      action = granit::window_loop_action::stop;
      return smoke_complete() || !smoke_test_ ? granit::result::success
                                              : granit::result::initialization_failed;
    }

    result = renderer_.process_events();
    if (result.failed())
      return result;
    if (phase_ == application_phase::renderer_initializing) {
      granit::renderer_status status;
      result = renderer_.get_status(status);
      if (result.failed())
        return result;
      if (status.state == granit::renderer_state::initializing) {
        action = granit::window_loop_action::idle;
        return granit::result::success;
      }
      if (status.state != granit::renderer_state::ready)
        return status.failure_result;
      result = initialize_presentation();
      if (result == granit::result::not_ready) {
        action = granit::window_loop_action::idle;
        return granit::result::success;
      }
      if (result.failed())
        return result;
      phase_ = application_phase::running;
    }

    result = window_.get_state(window_state_);
    if (result.failed())
      return result;
    const auto width = window_state_.framebuffer_width;
    const auto height = window_state_.framebuffer_height;
    if (width == 0 || height == 0) {
      action = granit::window_loop_action::idle;
      return granit::result::success;
    }
    if (recreate_ || width != swapchain_info_.width || height != swapchain_info_.height) {
      result = swapchain_.recreate({.width = width, .height = height});
      if (result == granit::result::not_ready) {
        action = granit::window_loop_action::idle;
        return granit::result::success;
      }
      if (result.failed())
        return result;
      result = swapchain_.query_info(swapchain_info_);
      if (result.failed())
        return result;
      recreate_ = false;
      ++completed_recreates_;
    }

    result = render_clear_frame(swapchain_, frame_context_, swapchain_info_, recreate_);
    if (result == granit::result::out_of_date) {
      recreate_ = true;
      return granit::result::success;
    }
    if (result.failed())
      return result;

    ++rendered_frames_;
    if (smoke_test_ && rendered_frames_ == 1)
      recreate_ = true;
    if (smoke_complete())
      action = granit::window_loop_action::stop;
    return granit::result::success;
  }

  void shutdown(granit::result) noexcept {
    phase_ = application_phase::stopped;
    static_cast<void>(frame_context_.reset());
    static_cast<void>(swapchain_.reset());
    static_cast<void>(surface_.reset());
    static_cast<void>(renderer_.reset());
    static_cast<void>(window_.reset());
    static_cast<void>(window_system_.reset());
  }

  [[nodiscard]] granit::window_system& system() noexcept { return window_system_; }
  [[nodiscard]] std::uint32_t rendered_frames() const noexcept { return rendered_frames_; }
  [[nodiscard]] std::uint32_t completed_recreates() const noexcept { return completed_recreates_; }
  [[nodiscard]] bool ready() const noexcept { return phase_ == application_phase::running; }

private:
  granit::result initialize_presentation() noexcept {
    auto result = window_.get_state(window_state_);
    if (result.failed())
      return result;
    if (window_state_.framebuffer_width == 0 || window_state_.framebuffer_height == 0)
      return granit::result::not_ready;
    result = window_.create_surface(renderer_, surface_);
    if (result.ok()) {
      result = swapchain_.initialize(
          renderer_, surface_,
          {.width = window_state_.framebuffer_width, .height = window_state_.framebuffer_height});
    }
    if (result.ok())
      result = swapchain_.query_info(swapchain_info_);
    if (result.ok())
      result = frame_context_.initialize(renderer_);
    return result;
  }

  [[nodiscard]] bool smoke_complete() const noexcept {
    return smoke_test_ && rendered_frames_ >= 3 && completed_recreates_ >= 1;
  }

  granit::window_system window_system_;
  granit::window window_;
  granit::renderer renderer_;
  granit::surface surface_;
  granit::swapchain swapchain_;
  granit::frame_context frame_context_;
  granit::window_state window_state_{};
  granit::swapchain_info swapchain_info_{};
  application_phase phase_{application_phase::renderer_initializing};
  bool running_{true};
  bool recreate_{};
  bool smoke_test_{};
  std::uint32_t rendered_frames_{};
  std::uint32_t completed_recreates_{};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_01_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_01_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_01_ready() noexcept {
  return application.ready() ? 1 : 0;
}
#endif

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";

  auto result = application.initialize(smoke_test);
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application initialize", result);
  result = granit::run_window_loop(application.system(), application);
  return result.failed() ? report_failure("application loop", result) : 0;
}
