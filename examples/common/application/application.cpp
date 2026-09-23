// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"

namespace granit::example {

result application::run(const application_desc& desc) noexcept {
  if (phase_ != phase::fresh || desc.title.empty() || desc.application_name.empty() ||
      desc.width == 0 || desc.height == 0 || desc.smoke_test_frames == 0) {
    return result::invalid_argument;
  }

  desc_ = desc;
  auto operation = window_system_.initialize();
  if (operation.ok()) {
    operation = window_.initialize(
        window_system_, {.title = desc.title, .width = desc.width, .height = desc.height});
  }
  if (operation.ok()) {
    operation = renderer_.initialize(
        {.application_name = desc.application_name, .presentation = presentation_mode::enabled});
  }
  if (operation.failed()) {
    shutdown(operation);
    return operation;
  }

  phase_ = phase::renderer_initializing;
  return run_window_loop(window_system_, *this);
}

result application::poll_events() noexcept {
  window_event window_event_value;
  result operation;
  while ((operation = window_system_.poll(window_event_value)).ok()) {
    if (window_event_value.window.native_handle() != window_.native_handle())
      continue;
    if (window_event_value.type == window_event_type::close_requested)
      running_ = false;
    if (window_event_value.type == window_event_type::resized ||
        window_event_value.type == window_event_type::scale_changed ||
        window_event_value.type == window_event_type::native_handle_changed) {
      recreate_ = true;
    }
    operation = on_window_event(window_event_value);
    if (operation.failed())
      return operation;
  }
  if (operation != result::not_ready)
    return operation;

  input_event input_event_value;
  while ((operation = window_system_.poll(input_event_value)).ok()) {
    if (input_event_value.window.native_handle() != window_.native_handle())
      continue;
    if (input_event_value.type == input_event_type::key &&
        input_event_value.data.key.action == key_action::released &&
        input_event_value.data.key.physical == physical_key::escape) {
      running_ = false;
    }
    operation = on_input_event(input_event_value);
    if (operation.failed())
      return operation;
  }
  return operation == result::not_ready ? result::success : operation;
}

result application::initialize_presentation() noexcept {
  auto operation = window_.get_state(window_state_);
  if (operation.failed())
    return operation;
  if (window_state_.framebuffer_width == 0 || window_state_.framebuffer_height == 0)
    return result::not_ready;

  if (!surface_.valid())
    operation = window_.create_surface(renderer_, surface_);
  if (operation.ok() && !swapchain_.valid()) {
    operation = swapchain_.initialize(
        renderer_, surface_,
        {.width = window_state_.framebuffer_width, .height = window_state_.framebuffer_height});
  }
  if (operation.ok())
    operation = swapchain_.query_info(swapchain_info_);
  if (operation.failed())
    return operation;

  content_started_ = true;
  operation = on_initialize();
  if (operation.ok()) {
    previous_frame_time_ = std::chrono::steady_clock::now();
    phase_ = phase::running;
  }
  return operation;
}

result application::update_presentation(window_loop_action& action) noexcept {
  auto operation = window_.get_state(window_state_);
  if (operation.failed())
    return operation;

  const auto width = window_state_.framebuffer_width;
  const auto height = window_state_.framebuffer_height;
  if (width == 0 || height == 0) {
    action = window_loop_action::idle;
    return result::not_ready;
  }
  if (!recreate_ && width == swapchain_info_.width && height == swapchain_info_.height)
    return result::success;

  operation = swapchain_.recreate({.width = width, .height = height});
  if (operation.ok())
    operation = swapchain_.query_info(swapchain_info_);
  if (operation.ok())
    operation = on_swapchain_changed(swapchain_info_);
  if (operation.ok()) {
    recreate_ = false;
    ++completed_recreates_;
  }
  return operation;
}

result application::render_present_frame() noexcept {
  acquired_frame acquired;
  auto operation = swapchain_.acquire(acquired);
  if (operation.failed())
    return operation;
  recreate_ = recreate_ || acquired.needs_recreate();

  swapchain_backbuffer backbuffer;
  operation = swapchain_.backbuffer(acquired, backbuffer);
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration<float>(now - previous_frame_time_).count();
  previous_frame_time_ = now;
  present_frame frame{.acquired = acquired,
                      .backbuffer = backbuffer,
                      .swapchain = swapchain_info_,
                      .delta_seconds = elapsed};
  if (operation.ok())
    operation = on_render(frame);
  if (operation.ok())
    operation = swapchain_.present(acquired);

  recreate_ = recreate_ || acquired.needs_recreate();
  if (operation.failed() && acquired.valid())
    static_cast<void>(swapchain_.cancel(acquired));
  return operation;
}

result application::tick(window_loop_action& action) noexcept {
  auto operation = poll_events();
  if (operation.failed())
    return operation;
  if (!running_) {
    action = window_loop_action::stop;
    return smoke_complete() || !desc_.smoke_test ? result::success
                                                 : result::initialization_failed;
  }

  operation = renderer_.process_events();
  if (operation.failed())
    return operation;
  if (phase_ == phase::renderer_initializing) {
    renderer_status status;
    operation = renderer_.get_status(status);
    if (operation.failed())
      return operation;
    if (status.state == renderer_state::initializing) {
      action = window_loop_action::idle;
      return result::success;
    }
    if (status.state != renderer_state::ready)
      return status.failure_result;
    operation = initialize_presentation();
    if (operation == result::not_ready) {
      action = window_loop_action::idle;
      return result::success;
    }
    if (operation.failed())
      return operation;
  }

  operation = update_presentation(action);
  if (operation == result::not_ready)
    return result::success;
  if (operation.failed())
    return operation;

  operation = render_present_frame();
  if (operation == result::out_of_date || operation == result::not_ready) {
    recreate_ = true;
    return result::success;
  }
  if (operation.failed())
    return operation;

  ++rendered_frames_;
  if (desc_.smoke_test && rendered_frames_ == 1)
    recreate_ = true;
  if (smoke_complete())
    action = window_loop_action::stop;
  return result::success;
}

void application::shutdown(result reason) noexcept {
  if (phase_ == phase::stopped)
    return;
  phase_ = phase::stopped;
  if (content_started_)
    on_shutdown(reason);
  static_cast<void>(swapchain_.reset());
  static_cast<void>(surface_.reset());
  static_cast<void>(renderer_.reset());
  static_cast<void>(window_.reset());
  static_cast<void>(window_system_.reset());
}

bool application::ready() const noexcept { return phase_ == phase::running; }

result application::on_swapchain_changed(const swapchain_info&) noexcept {
  return result::success;
}

result application::on_window_event(const window_event&) noexcept { return result::success; }

result application::on_input_event(const input_event&) noexcept { return result::success; }

void application::on_shutdown(result) noexcept {}

bool application::smoke_complete() const noexcept {
  return desc_.smoke_test && rendered_frames_ >= desc_.smoke_test_frames &&
         completed_recreates_ >= 1;
}

} // namespace granit::example
