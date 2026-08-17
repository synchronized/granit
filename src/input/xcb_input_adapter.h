// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INPUT_XCB_INPUT_ADAPTER_H_
#define GRANIT_INPUT_XCB_INPUT_ADAPTER_H_

#include <granit/input/input.h>

#include <cstdint>

namespace granit::input::detail {

struct xcb_input_event {
  std::uint32_t type{};
  std::int32_t x{};
  std::int32_t y{};
  std::uint32_t state{};
  std::uint32_t detail{};
};

struct xcb_input_sink {
  void* user_data{};
  granit_keyboard_state& (*keyboard)(void* user_data, granit_window window);
  granit_pointer_state& (*pointer)(void* user_data, granit_window window);
  void (*event)(void* user_data, granit_window window, std::uint32_t type,
                const granit_input_event_data& data);
};

void handle_xcb_input(granit_window window, const xcb_input_event& event,
                      const xcb_input_sink& sink);

} // namespace granit::input::detail

#endif
