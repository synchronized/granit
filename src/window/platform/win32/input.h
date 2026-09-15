// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PLATFORM_WIN32_INPUT_ADAPTER_H_
#define GRANIT_PLATFORM_WIN32_INPUT_ADAPTER_H_

#include <granit/input/input.h>

#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace granit::input::detail {

struct win32_input_sink {
  void* user_data{};
  granit_keyboard_state& (*keyboard)(void* user_data, granit_window window);
  granit_pointer_state& (*pointer)(void* user_data, granit_window window);
  void (*event)(void* user_data, granit_window window, std::uint32_t type,
                const granit_input_event_data& data);
  void (*text)(void* user_data, granit_window window, std::string_view text);
};

class win32_input_adapter {
public:
  void handle(granit_window window, std::uint32_t message, std::uintptr_t word, std::intptr_t value,
              const win32_input_sink& sink);
  void clear_window(granit_window window) noexcept;

private:
  std::unordered_map<granit_window, wchar_t> pending_high_surrogates_;
};

} // namespace granit::input::detail

#endif
