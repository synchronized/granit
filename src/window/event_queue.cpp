// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/window.h>

#include "window/event_queue.h"

#include <chrono>
#include <cstdint>

namespace granit::window::detail {

std::uint64_t timestamp_ns() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
}

void enqueue_event(const std::shared_ptr<window_system_record>& system, granit_window window,
                   std::uint32_t type) {
  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  event.type = type;
  event.window = window;
  event.timestamp_ns = timestamp_ns();
  system->events.push_back(event);
}

} // namespace granit::window::detail

using namespace granit::window::detail;

extern "C" granit_result granit_window_poll_event(granit_window_system handle,
                                                  granit_window_event* event) {
  if (event == nullptr || event->struct_size < GRANIT_WINDOW_EVENT_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (system->events.empty())
    return GRANIT_ERROR_NOT_READY;
  *event = system->events.front();
  system->events.pop_front();
  return GRANIT_SUCCESS;
}
