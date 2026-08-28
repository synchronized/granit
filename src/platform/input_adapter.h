// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PLATFORM_INPUT_ADAPTER_H_
#define GRANIT_PLATFORM_INPUT_ADAPTER_H_

#include <granit/input/input.h>

#include "platform/window/input_bridge.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace granit::input::detail {

struct platform_input_sink {
  void* user_data{};
  granit_keyboard_state& (*keyboard)(void* user_data, granit_window window);
  granit_pointer_state& (*pointer)(void* user_data, granit_window window);
  void (*event)(void* user_data, granit_window window, std::uint32_t type,
                const granit_input_event_data& data);
  void (*text)(void* user_data, granit_window window, std::string_view text);
};

/** 将窗口系统原生事件分派到当前构建启用的平台输入解码器。 */
class platform_input_adapter {
public:
  platform_input_adapter();
  ~platform_input_adapter();
  platform_input_adapter(const platform_input_adapter&) = delete;
  platform_input_adapter& operator=(const platform_input_adapter&) = delete;

  void handle(granit_window window, const granit_window_input_native_event& event,
              const platform_input_sink& sink);
  void clear_window(granit_window window) noexcept;

private:
  struct implementation;
  std::unique_ptr<implementation> implementation_;
};

} // namespace granit::input::detail

#endif
