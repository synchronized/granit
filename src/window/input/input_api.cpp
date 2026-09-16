// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/input.h>

#include "window/registry.h"

#include <algorithm>
#include <cstring>

namespace {

template <typename T> void write_output(T* output, uint32_t capacity, T value) {
  const auto written = std::min<std::size_t>(capacity, sizeof(T));
  value.struct_size = static_cast<uint32_t>(written);
  std::memcpy(output, &value, written);
}

} // namespace

using namespace granit::window::detail;

extern "C" granit_result granit_window_poll_input_event(granit_window_system handle,
                                                        granit_input_event* event) {
  if (event == nullptr || event->struct_size < GRANIT_INPUT_EVENT_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto capacity = event->struct_size;
  write_output(event, capacity, granit_input_event GRANIT_INPUT_EVENT_INIT);
  const auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (system->input_events.empty())
    return GRANIT_ERROR_NOT_READY;
  write_output(event, capacity, system->input_events.front());
  system->input_events.pop_front();
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_window_get_keyboard_state(granit_window_system handle,
                                                          granit_window window,
                                                          granit_keyboard_state* state) {
  if (state == nullptr || state->struct_size < GRANIT_KEYBOARD_STATE_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto capacity = state->struct_size;
  write_output(state, capacity, granit_keyboard_state GRANIT_KEYBOARD_STATE_INIT);
  const auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (!system->windows.contains(window))
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto found = system->keyboards.find(window); found != system->keyboards.end())
    write_output(state, capacity, found->second);
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_window_get_pointer_state(granit_window_system handle,
                                                         granit_window window,
                                                         granit_pointer_state* state) {
  if (state == nullptr || state->struct_size < GRANIT_POINTER_STATE_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto capacity = state->struct_size;
  write_output(state, capacity, granit_pointer_state GRANIT_POINTER_STATE_INIT);
  const auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (!system->windows.contains(window))
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto found = system->pointers.find(window); found != system->pointers.end())
    write_output(state, capacity, found->second);
  return GRANIT_SUCCESS;
}
