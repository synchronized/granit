// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/input.h>

#include "core/utf8.h"
#include "window/window_backend_internal.h"

#include <algorithm>
#include <cstring>
#include <string_view>

namespace {

template <typename T> void write_output(T* output, uint32_t capacity, T value) {
  const auto written = std::min<std::size_t>(capacity, sizeof(T));
  value.struct_size = static_cast<uint32_t>(written);
  std::memcpy(output, &value, written);
}

granit_keyboard_state& keyboard_for(granit::window::detail::window_system_record& system,
                                    granit_window window) {
  auto& state = system.keyboards[window];
  if (state.struct_size == 0)
    state = GRANIT_KEYBOARD_STATE_INIT;
  return state;
}

granit_pointer_state& pointer_for(granit::window::detail::window_system_record& system,
                                  granit_window window) {
  auto& state = system.pointers[window];
  if (state.struct_size == 0)
    state = GRANIT_POINTER_STATE_INIT;
  return state;
}

void enqueue(granit::window::detail::window_system_record& system, granit_window window,
             std::uint32_t type, const granit_input_event_data& data = {}) {
  granit_input_event event = GRANIT_INPUT_EVENT_INIT;
  event.type = type;
  event.window = window;
  event.timestamp_ns = granit::window::detail::timestamp_ns();
  event.data = data;
  system.input_events.push_back(event);
}

void enqueue_text(granit::window::detail::window_system_record& system, granit_window window,
                  std::string_view text) {
  std::size_t validated_length = 0;
  if (granit::detail::next_utf8_chunk(text, text.size(), validated_length) !=
          granit::detail::utf8_chunk_result::success ||
      validated_length != text.size()) {
    return;
  }
  while (!text.empty()) {
    std::size_t length = 0;
    const auto result = granit::detail::next_utf8_chunk(text, GRANIT_INPUT_TEXT_CAPACITY, length);
    if (result != granit::detail::utf8_chunk_result::success || length == 0)
      return;
    granit_input_event_data data{};
    data.text.length = static_cast<std::uint32_t>(length);
    std::memcpy(data.text.utf8, text.data(), length);
    enqueue(system, window, GRANIT_INPUT_EVENT_TEXT, data);
    text.remove_prefix(length);
  }
}

granit_keyboard_state& adapter_keyboard(void* user_data, granit_window window) {
  return keyboard_for(*static_cast<granit::window::detail::window_system_record*>(user_data),
                      window);
}

granit_pointer_state& adapter_pointer(void* user_data, granit_window window) {
  return pointer_for(*static_cast<granit::window::detail::window_system_record*>(user_data),
                     window);
}

void adapter_event(void* user_data, granit_window window, std::uint32_t type,
                   const granit_input_event_data& data) {
  enqueue(*static_cast<granit::window::detail::window_system_record*>(user_data), window, type,
          data);
}

void adapter_text(void* user_data, granit_window window, std::string_view text) {
  enqueue_text(*static_cast<granit::window::detail::window_system_record*>(user_data), window,
               text);
}

} // namespace

namespace granit::window::detail {

void handle_native_input(window_system_record& system, granit_window window,
                         const granit_window_input_native_event& event) {
  try {
    const granit::input::detail::platform_input_sink sink{
        &system, adapter_keyboard, adapter_pointer, adapter_event, adapter_text};
    system.input_platform.handle(window, event, sink);
  } catch (...) {
    // 平台事件边界不能传播分配异常；丢弃当前输入，后续状态仍可继续更新。
  }
}

void clear_window_input(window_system_record& system, granit_window window) {
  system.keyboards.erase(window);
  system.pointers.erase(window);
  system.input_platform.clear_window(window);
  std::erase_if(system.input_events,
                [window](const auto& event) { return event.window == window; });
}

void clear_input_focus(window_system_record& system, granit_window window) {
  if (const auto keyboard = system.keyboards.find(window); keyboard != system.keyboards.end())
    keyboard->second = GRANIT_KEYBOARD_STATE_INIT;
  if (const auto pointer = system.pointers.find(window); pointer != system.pointers.end())
    pointer->second.buttons = 0;
  system.input_platform.clear_window(window);
}

} // namespace granit::window::detail

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
