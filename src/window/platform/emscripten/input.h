// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PLATFORM_EMSCRIPTEN_INPUT_ADAPTER_H_
#define GRANIT_PLATFORM_EMSCRIPTEN_INPUT_ADAPTER_H_

#include <granit/window/input.h>

#include "window/input/native_event.h"

#include <cstdint>
#include <string_view>

namespace granit::input::detail {

struct emscripten_input_sink {
  void* user_data{};
  granit_keyboard_state& (*keyboard)(void* user_data, granit_window window);
  granit_pointer_state& (*pointer)(void* user_data, granit_window window);
  void (*event)(void* user_data, granit_window window, std::uint32_t type,
                const granit_input_event_data& data);
  void (*text)(void* user_data, granit_window window, std::string_view text);
};

class emscripten_input_adapter {
public:
  void handle(granit_window window, const granit_window_input_native_event& event,
              const emscripten_input_sink& sink);
  void clear_window(granit_window window) noexcept;
};

} // namespace granit::input::detail

#endif
