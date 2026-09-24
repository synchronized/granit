// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"

namespace granit::example {

result application::run(const application_desc& desc) noexcept {
  if (phase_ != phase::fresh || desc.renderer.application_name.empty() ||
      desc.renderer.presentation != presentation_mode::enabled || desc.smoke_test_frames == 0) {
    return result::invalid_argument;
  }

  desc_ = desc;
  return run_host({.executable_path = desc.executable_path,
                   .title = desc.title,
                   .window_system = {},
                   .window_flags = window_flag::visible | window_flag::resizable,
                   .width = desc.width,
                   .height = desc.height});
}

result application::on_host_initialize() noexcept {
  phase_ = phase::renderer_initializing;
  return renderer_.initialize(desc_.renderer);
}

result application::initialize_presentation() noexcept {
  auto operation = app_window().get_state(window_state_);
  if (operation.failed())
    return operation;
  if (window_state_.framebuffer_width == 0 || window_state_.framebuffer_height == 0)
    return result::not_ready;

  if (!surface_.valid())
    operation = app_window().create_surface(renderer_, surface_);
  if (operation.ok() && !swapchain_.valid()) {
    auto swapchain_desc = desc_.swapchain;
    swapchain_desc.width = window_state_.framebuffer_width;
    swapchain_desc.height = window_state_.framebuffer_height;
    operation = swapchain_.initialize(renderer_, surface_, swapchain_desc);
  }
  if (operation.ok())
    operation = swapchain_.query_info(swapchain_info_);
  if (operation.failed())
    return operation;

  content_started_ = true;
  operation = on_initialize();
  if (operation.ok())
    phase_ = phase::running;
  return operation;
}

result application::update_presentation(window_loop_action& action) noexcept {
  auto operation = app_window().get_state(window_state_);
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

  auto swapchain_desc = desc_.swapchain;
  swapchain_desc.width = width;
  swapchain_desc.height = height;
  operation = swapchain_.recreate(swapchain_desc);
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
  present_frame frame{.acquired = acquired,
                      .backbuffer = backbuffer,
                      .swapchain = swapchain_info_,
                      .delta_seconds = delta_seconds_};
  if (operation.ok())
    operation = on_render(frame);
  if (operation.ok())
    operation = swapchain_.present(acquired);

  recreate_ = recreate_ || acquired.needs_recreate();
  if (operation.failed() && acquired.valid())
    static_cast<void>(swapchain_.cancel(acquired));
  return operation;
}

result application::on_host_update(float delta_seconds, window_loop_action& action) noexcept {
  auto operation = renderer_.process_events();
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

  delta_seconds_ = delta_seconds;
  operation = on_update(delta_seconds_);
  if (operation.failed())
    return operation;

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

result application::on_host_window_event(const window_event& event) noexcept {
  if (event.type == window_event_type::resized || event.type == window_event_type::scale_changed ||
      event.type == window_event_type::native_handle_changed) {
    recreate_ = true;
  }
  return content_started_ ? on_window_event(event) : result::success;
}

result application::on_host_input_event(const input_event& event) noexcept {
  return content_started_ ? on_input_event(event) : result::success;
}

void application::on_host_shutdown(result reason) noexcept {
  if (phase_ == phase::stopped)
    return;
  phase_ = phase::stopped;
  if (content_started_)
    on_shutdown(reason);
  static_cast<void>(swapchain_.reset());
  static_cast<void>(surface_.reset());
  static_cast<void>(renderer_.reset());
}

bool application::ready() const noexcept { return phase_ == phase::running; }

result application::on_update(float) noexcept { return result::success; }

result application::on_swapchain_changed(const swapchain_info&) noexcept { return result::success; }

result application::on_window_event(const window_event&) noexcept { return result::success; }

result application::on_input_event(const input_event&) noexcept { return result::success; }

void application::on_shutdown(result) noexcept {}

bool application::smoke_complete() const noexcept {
  return desc_.smoke_test && rendered_frames_ >= desc_.smoke_test_frames &&
         completed_recreates_ >= 1;
}

} // namespace granit::example
