// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application_host.h"

#include <new>

namespace granit::example {

result application_host::run_host(const application_host_desc& desc) noexcept {
  if (phase_ != phase::fresh || desc.executable_path.empty() || desc.title.empty() ||
      desc.width == 0 || desc.height == 0) {
    return result::invalid_argument;
  }
  if (!assets_.initialize(desc.executable_path))
    return result::invalid_argument;

  auto operation = window_system_.initialize(desc.window_system);
  if (operation.ok()) {
    operation = window_.initialize(window_system_, {.title = desc.title,
                                                    .width = desc.width,
                                                    .height = desc.height,
                                                    .flags = desc.window_flags});
  }
  if (operation.ok())
    operation = on_host_initialize();
  if (operation.failed()) {
    shutdown(operation);
    return operation;
  }

  phase_ = phase::running;
  previous_tick_time_ = std::chrono::steady_clock::now();
  return run_window_loop(window_system_, *this);
}

result application_host::poll_events() noexcept {
  window_event window_event_value;
  result operation;
  while ((operation = window_system_.poll(window_event_value)).ok()) {
    if (window_event_value.window.native_handle() != window_.native_handle())
      continue;
    if (window_event_value.type == window_event_type::close_requested)
      running_ = false;
    operation = on_host_window_event(window_event_value);
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
    operation = on_host_input_event(input_event_value);
    if (operation.failed())
      return operation;
  }
  return operation == result::not_ready ? result::success : operation;
}

result application_host::update_services() noexcept {
  try {
    asset_loader_.poll();
    return result::success;
  } catch (const std::bad_alloc&) {
    return result::out_of_memory;
  } catch (...) {
    return result::internal;
  }
}

result application_host::tick(window_loop_action& action) noexcept {
  auto operation = update_services();
  if (operation.ok())
    operation = poll_events();
  if (operation.failed())
    return operation;
  if (!running_) {
    action = window_loop_action::stop;
    return result::success;
  }

  const auto now = std::chrono::steady_clock::now();
  const auto delta_seconds = std::chrono::duration<float>(now - previous_tick_time_).count();
  previous_tick_time_ = now;
  return on_host_update(delta_seconds, action);
}

void application_host::shutdown(result reason) noexcept {
  if (phase_ == phase::stopped)
    return;
  phase_ = phase::stopped;
  on_host_shutdown(reason);
  static_cast<void>(window_.reset());
  static_cast<void>(window_system_.reset());
}

result application_host::on_host_window_event(const window_event&) noexcept {
  return result::success;
}

result application_host::on_host_input_event(const input_event&) noexcept {
  return result::success;
}

void application_host::on_host_shutdown(result) noexcept {}

} // namespace granit::example
