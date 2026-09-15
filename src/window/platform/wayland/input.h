// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PLATFORM_WAYLAND_INPUT_ADAPTER_H_
#define GRANIT_PLATFORM_WAYLAND_INPUT_ADAPTER_H_

#include <granit/input/input.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace granit::input::detail {

struct wayland_input_sink {
  void* user_data{};
  granit_keyboard_state& (*keyboard)(void* user_data, granit_window window);
  granit_pointer_state& (*pointer)(void* user_data, granit_window window);
  void (*event)(void* user_data, granit_window window, std::uint32_t type,
                const granit_input_event_data& data);
  void (*text)(void* user_data, granit_window window, std::string_view text);
};

class wayland_input_adapter {
public:
  wayland_input_adapter();
  ~wayland_input_adapter();
  wayland_input_adapter(const wayland_input_adapter&) = delete;
  wayland_input_adapter& operator=(const wayland_input_adapter&) = delete;

  bool set_keymap(const char* text, std::size_t length) noexcept;
  void modifiers(granit_window window, std::uint32_t depressed, std::uint32_t latched,
                 std::uint32_t locked, std::uint32_t group,
                 const wayland_input_sink& sink) noexcept;
  void key(granit_window window, std::uint32_t key, bool pressed, const wayland_input_sink& sink);
  void pointer_enter(granit_window window, float x, float y, const wayland_input_sink& sink);
  void pointer_leave(granit_window window, const wayland_input_sink& sink);
  void pointer_motion(granit_window window, float x, float y, const wayland_input_sink& sink);
  void pointer_button(granit_window window, std::uint32_t button, bool pressed,
                      const wayland_input_sink& sink);
  void pointer_axis(granit_window window, std::uint32_t axis, float value,
                    const wayland_input_sink& sink);
  void clear_window(granit_window window) noexcept;

private:
  struct implementation;
  implementation* implementation_{};
};

} // namespace granit::input::detail

#endif
