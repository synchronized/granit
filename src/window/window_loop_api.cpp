// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/window_loop.h>

#include "window/registry.h"

#include <chrono>
#include <thread>

using namespace granit::window::detail;

namespace {

bool valid_desc(const granit_window_loop_desc* desc) noexcept {
  return desc != nullptr && desc->struct_size >= GRANIT_WINDOW_LOOP_DESC_VERSION_1_SIZE &&
         desc->tick != nullptr && desc->shutdown != nullptr && desc->flags == 0 &&
         desc->reserved == 0;
}

} // namespace

extern "C" granit_result
granit_window_system_run_loop(granit_window_system system_handle,
                              const granit_window_loop_desc* desc) {
  if (!valid_desc(desc))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (system->loop_running)
    return GRANIT_ERROR_RESOURCE_IN_USE;

#if defined(__EMSCRIPTEN__)
  return GRANIT_ERROR_UNSUPPORTED;
#else
  const auto callbacks = *desc;
  system->loop_running = true;
  auto reason = GRANIT_SUCCESS;
  while (reason == GRANIT_SUCCESS) {
    reason = granit_window_system_process_events(system_handle);
    if (reason != GRANIT_SUCCESS)
      break;

    granit_window_loop_action action = GRANIT_WINDOW_LOOP_CONTINUE;
    reason = callbacks.tick(callbacks.user_data, &action);
    if (reason != GRANIT_SUCCESS)
      break;
    if (action == GRANIT_WINDOW_LOOP_STOP)
      break;
    if (action != GRANIT_WINDOW_LOOP_CONTINUE && action != GRANIT_WINDOW_LOOP_IDLE) {
      reason = GRANIT_ERROR_INVALID_ARGUMENT;
      break;
    }
    if (action == GRANIT_WINDOW_LOOP_IDLE)
      std::this_thread::sleep_for(std::chrono::milliseconds{16});
  }
  system->loop_running = false;
  callbacks.shutdown(callbacks.user_data, reason);
  return reason;
#endif
}
