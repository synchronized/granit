// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_LOOP_HPP_
#define GRANIT_WINDOW_LOOP_HPP_

#include <concepts>

#include <granit/core/result.hpp>
#include <granit/window/window.hpp>
#include <granit/window/window_loop.h>

namespace granit {

enum class window_loop_action : std::uint32_t {
  continue_running = GRANIT_WINDOW_LOOP_CONTINUE,
  idle = GRANIT_WINDOW_LOOP_IDLE,
  stop = GRANIT_WINDOW_LOOP_STOP,
};

template <typename Application>
concept window_loop_application = requires(Application& application, window_loop_action& action,
                                           result reason) {
  { application.tick(action) } noexcept -> std::same_as<result>;
  { application.shutdown(reason) } noexcept -> std::same_as<void>;
};

/** 在 Window System 创建线程运行应用 Tick；Application 必须持续有效到 shutdown 返回。 */
template <window_loop_application Application>
[[nodiscard]] result run_window_loop(window_system& system, Application& application) noexcept {
  granit_window_loop_desc desc = GRANIT_WINDOW_LOOP_DESC_INIT;
  desc.user_data = &application;
  desc.tick = [](void* user_data, granit_window_loop_action* native_action) noexcept {
    auto action = window_loop_action::continue_running;
    const auto tick_result = static_cast<Application*>(user_data)->tick(action);
    *native_action = static_cast<granit_window_loop_action>(action);
    return tick_result.native();
  };
  desc.shutdown = [](void* user_data, granit_result reason) noexcept {
    static_cast<Application*>(user_data)->shutdown(from_native(reason));
  };
  return from_native(granit_window_system_run_loop(system.native_handle(), &desc));
}

} // namespace granit

#endif
