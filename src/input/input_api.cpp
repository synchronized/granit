// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/input/input.h>

#include "platform/input_adapter.h"
#include "utf8.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace {

struct input_record {
  granit_window_system window_system{};
  std::thread::id owner_thread;
  std::deque<granit_input_event> events;
  std::unordered_map<granit_window, granit_keyboard_state> keyboards;
  std::unordered_map<granit_window, granit_pointer_state> pointers;
  granit::input::detail::platform_input_adapter platform;
};

std::mutex registry_mutex;
std::unordered_map<granit_input_system, std::shared_ptr<input_record>> inputs;
std::atomic<std::uint64_t> next_handle{UINT64_C(1)};

template <typename T> void write_output(T* output, uint32_t capacity, T value) {
  const auto written = std::min<std::size_t>(capacity, sizeof(T));
  value.struct_size = static_cast<uint32_t>(written);
  std::memcpy(output, &value, written);
}

std::shared_ptr<input_record> acquire(granit_input_system handle) {
  std::lock_guard lock{registry_mutex};
  const auto found = inputs.find(handle);
  return found == inputs.end() ? nullptr : found->second;
}

std::uint64_t timestamp_ns() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
}

granit_keyboard_state& keyboard_for(input_record& input, granit_window window) {
  auto& state = input.keyboards[window];
  if (state.struct_size == 0)
    state = GRANIT_KEYBOARD_STATE_INIT;
  return state;
}

granit_pointer_state& pointer_for(input_record& input, granit_window window) {
  auto& state = input.pointers[window];
  if (state.struct_size == 0)
    state = GRANIT_POINTER_STATE_INIT;
  return state;
}

void enqueue(input_record& input, granit_window window, std::uint32_t type,
             const granit_input_event_data& data = {}) {
  granit_input_event event = GRANIT_INPUT_EVENT_INIT;
  event.type = type;
  event.window = window;
  event.timestamp_ns = timestamp_ns();
  event.data = data;
  input.events.push_back(event);
}

void enqueue_text(input_record& input, granit_window window, std::string_view text) {
  std::size_t validated_length = 0;
  if (granit::input::detail::next_utf8_chunk(text, text.size(), validated_length) !=
          granit::input::detail::utf8_chunk_result::success ||
      validated_length != text.size()) {
    return;
  }
  while (!text.empty()) {
    std::size_t length = 0;
    const auto result =
        granit::input::detail::next_utf8_chunk(text, GRANIT_INPUT_TEXT_CAPACITY, length);
    if (result != granit::input::detail::utf8_chunk_result::success || length == 0)
      return;
    granit_input_event_data data{};
    data.text.length = static_cast<std::uint32_t>(length);
    std::memcpy(data.text.utf8, text.data(), length);
    enqueue(input, window, GRANIT_INPUT_EVENT_TEXT, data);
    text.remove_prefix(length);
  }
}

void clear_window(void* user_data, granit_window window) {
  auto& input = *static_cast<input_record*>(user_data);
  input.keyboards.erase(window);
  input.pointers.erase(window);
  input.platform.clear_window(window);
  std::erase_if(input.events, [window](const auto& event) { return event.window == window; });
}

void clear_focus(void* user_data, granit_window window) {
  auto& input = *static_cast<input_record*>(user_data);
  if (const auto keyboard = input.keyboards.find(window); keyboard != input.keyboards.end())
    keyboard->second = GRANIT_KEYBOARD_STATE_INIT;
  if (const auto pointer = input.pointers.find(window); pointer != input.pointers.end())
    pointer->second.buttons = 0;
  input.platform.clear_window(window);
}

granit_keyboard_state& adapter_keyboard(void* user_data, granit_window window) {
  return keyboard_for(*static_cast<input_record*>(user_data), window);
}

granit_pointer_state& adapter_pointer(void* user_data, granit_window window) {
  return pointer_for(*static_cast<input_record*>(user_data), window);
}

void adapter_event(void* user_data, granit_window window, std::uint32_t type,
                   const granit_input_event_data& data) {
  enqueue(*static_cast<input_record*>(user_data), window, type, data);
}

void adapter_text(void* user_data, granit_window window, std::string_view text) {
  enqueue_text(*static_cast<input_record*>(user_data), window, text);
}

void native_event(void* user_data, granit_window window,
                  const granit_window_input_native_event* event) {
  if (event == nullptr)
    return;
  auto& input = *static_cast<input_record*>(user_data);
  try {
    const granit::input::detail::platform_input_sink sink{&input, adapter_keyboard, adapter_pointer,
                                                          adapter_event, adapter_text};
    input.platform.handle(window, *event, sink);
  } catch (...) {
    // 平台回调不能让分配异常穿过 DLL 或原生事件边界。
  }
}

} // namespace

extern "C" granit_result granit_input_system_create(const granit_input_system_desc* desc,
                                                    granit_input_system* output) {
  if (output == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_INPUT_SYSTEM_DESC_VERSION_1_SIZE ||
      desc->flags != 0 || desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc->window_system == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    auto input = std::make_shared<input_record>();
    input->window_system = desc->window_system;
    input->owner_thread = std::this_thread::get_id();
    const auto handle = next_handle.fetch_add(1, std::memory_order_relaxed);
    {
      std::lock_guard lock{registry_mutex};
      inputs.emplace(handle, input);
    }
    const auto result = granit_window_internal_attach_input(
        desc->window_system, input.get(), clear_window, clear_focus, native_event);
    if (result != GRANIT_SUCCESS) {
      std::lock_guard lock{registry_mutex};
      inputs.erase(handle);
      return result;
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
  if (event == nullptr || event->struct_size < GRANIT_INPUT_EVENT_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto capacity = event->struct_size;
  write_output(event, capacity, granit_input_event GRANIT_INPUT_EVENT_INIT);
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
  write_output(event, capacity, input->events.front());
  input->events.pop_front();
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_input_get_keyboard_state(granit_input_system handle,
                                                         granit_window window,
                                                         granit_keyboard_state* state) {
  if (state == nullptr || state->struct_size < GRANIT_KEYBOARD_STATE_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto capacity = state->struct_size;
  write_output(state, capacity, granit_keyboard_state GRANIT_KEYBOARD_STATE_INIT);
  const auto input = acquire(handle);
  if (!input)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (input->owner_thread != std::this_thread::get_id())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto valid = granit_window_internal_contains(input->window_system, window);
  if (valid != GRANIT_SUCCESS)
    return valid;
  if (const auto found = input->keyboards.find(window); found != input->keyboards.end())
    write_output(state, capacity, found->second);
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_input_get_pointer_state(granit_input_system handle,
                                                        granit_window window,
                                                        granit_pointer_state* state) {
  if (state == nullptr || state->struct_size < GRANIT_POINTER_STATE_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto capacity = state->struct_size;
  write_output(state, capacity, granit_pointer_state GRANIT_POINTER_STATE_INIT);
  const auto input = acquire(handle);
  if (!input)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (input->owner_thread != std::this_thread::get_id())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto valid = granit_window_internal_contains(input->window_system, window);
  if (valid != GRANIT_SUCCESS)
    return valid;
  if (const auto found = input->pointers.find(window); found != input->pointers.end())
    write_output(state, capacity, found->second);
  return GRANIT_SUCCESS;
}
