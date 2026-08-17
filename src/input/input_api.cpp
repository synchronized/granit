// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/input/input.h>

#include "../window/input_bridge.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace {
struct input_record {
  granit_window_system window_system{};
  std::thread::id owner_thread;
  std::deque<granit_input_event> events;
  std::unordered_map<granit_window, granit_keyboard_state> keyboards;
  std::unordered_map<granit_window, granit_pointer_state> pointers;
};
std::mutex registry_mutex;
std::unordered_map<granit_input_system, std::shared_ptr<input_record>> inputs;
std::atomic<std::uint64_t> next_handle{UINT64_C(1)};

std::shared_ptr<input_record> acquire(granit_input_system handle) {
  std::lock_guard lock{registry_mutex};
  const auto found = inputs.find(handle);
  return found == inputs.end() ? nullptr : found->second;
}
void clear_window(void* user_data, granit_window window) {
  auto& input = *static_cast<input_record*>(user_data);
  input.keyboards.erase(window);
  input.pointers.erase(window);
  std::erase_if(input.events, [window](const auto& event) { return event.window == window; });
}
void clear_focus(void* user_data, granit_window window) {
  auto& input = *static_cast<input_record*>(user_data);
  input.keyboards[window] = GRANIT_KEYBOARD_STATE_INIT;
  input.pointers[window].buttons = 0;
}
} // namespace

extern "C" granit_result granit_input_system_create(const granit_input_system_desc* desc,
                                                    granit_input_system* output) {
  if (output == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_INPUT_SYSTEM_DESC_VERSION_1_SIZE ||
      desc->window_system == GRANIT_NULL_HANDLE || desc->flags != 0 || desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto input = std::make_shared<input_record>();
    input->window_system = desc->window_system;
    input->owner_thread = std::this_thread::get_id();
    const auto result = granit_window_internal_attach_input(desc->window_system, input.get(),
                                                            clear_window, clear_focus);
    if (result != GRANIT_SUCCESS)
      return result;
    const auto handle = next_handle.fetch_add(1, std::memory_order_relaxed);
    {
      std::lock_guard lock{registry_mutex};
      inputs.emplace(handle, input);
    }
    *output = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_input_system_destroy(granit_input_system handle) {
  const auto input = acquire(handle);
  if (!input)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (input->owner_thread != std::this_thread::get_id())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = granit_window_internal_detach_input(input->window_system, input.get());
  if (result != GRANIT_SUCCESS)
    return result;
  std::lock_guard lock{registry_mutex};
  inputs.erase(handle);
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_input_poll_event(granit_input_system handle,
                                                 granit_input_event* event) {
  if (event == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *event = GRANIT_INPUT_EVENT_INIT;
  const auto input = acquire(handle);
  if (!input)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (input->owner_thread != std::this_thread::get_id())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = granit_window_internal_pump(input->window_system);
  if (result != GRANIT_SUCCESS)
    return result;
  if (input->events.empty())
    return GRANIT_ERROR_NOT_READY;
  *event = input->events.front();
  input->events.pop_front();
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_input_get_keyboard_state(granit_input_system handle,
                                                         granit_window window,
                                                         granit_keyboard_state* state) {
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *state = GRANIT_KEYBOARD_STATE_INIT;
  const auto input = acquire(handle);
  if (!input)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (input->owner_thread != std::this_thread::get_id())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto valid = granit_window_internal_contains(input->window_system, window);
  if (valid != GRANIT_SUCCESS)
    return valid;
  if (const auto found = input->keyboards.find(window); found != input->keyboards.end())
    *state = found->second;
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_input_get_pointer_state(granit_input_system handle,
                                                        granit_window window,
                                                        granit_pointer_state* state) {
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *state = GRANIT_POINTER_STATE_INIT;
  const auto input = acquire(handle);
  if (!input)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (input->owner_thread != std::this_thread::get_id())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto valid = granit_window_internal_contains(input->window_system, window);
  if (valid != GRANIT_SUCCESS)
    return valid;
  if (const auto found = input->pointers.find(window); found != input->pointers.end())
    *state = found->second;
  return GRANIT_SUCCESS;
}
