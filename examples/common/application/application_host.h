// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLE_APPLICATION_APPLICATION_HOST_H_
#define GRANIT_EXAMPLE_APPLICATION_APPLICATION_HOST_H_

#include "assets/asset_loader.h"
#include "assets/asset_store.h"

#include <chrono>
#include <cstdint>
#include <string_view>

#include <granit/window.hpp>

namespace granit::example {

struct application_host_desc {
  std::string_view executable_path;
  std::string_view title{"Granit Example"};
  window_system_desc window_system;
  window_flag window_flags{window_flag::visible | window_flag::resizable};
  std::uint32_t width{1280};
  std::uint32_t height{720};
};

/** 跨平台示例宿主，只管理 Window、事件循环和资产服务，不拥有 Renderer。 */
class application_host {
public:
  application_host() = default;
  virtual ~application_host() = default;

  application_host(const application_host&) = delete;
  application_host& operator=(const application_host&) = delete;

  [[nodiscard]] result run_host(const application_host_desc& desc) noexcept;

  // run_window_loop 使用的公开协议；示例不应直接调用。
  [[nodiscard]] result tick(window_loop_action& action) noexcept;
  void shutdown(result reason) noexcept;

protected:
  [[nodiscard]] virtual result on_host_initialize() noexcept = 0;
  [[nodiscard]] virtual result on_host_update(float delta_seconds,
                                              window_loop_action& action) noexcept = 0;
  [[nodiscard]] virtual result on_host_window_event(const window_event& event) noexcept;
  [[nodiscard]] virtual result on_host_input_event(const input_event& event) noexcept;
  virtual void on_host_shutdown(result reason) noexcept;

  [[nodiscard]] granit::window& app_window() noexcept { return window_; }
  [[nodiscard]] const granit::window& app_window() const noexcept { return window_; }
  [[nodiscard]] window_system& app_window_system() noexcept { return window_system_; }
  [[nodiscard]] assets::asset_store& assets() noexcept { return assets_; }
  [[nodiscard]] assets::asset_loader& asset_loader() noexcept { return asset_loader_; }

  void request_stop() noexcept { running_ = false; }

private:
  enum class phase { fresh, running, stopped };

  [[nodiscard]] result poll_events() noexcept;
  [[nodiscard]] result update_services() noexcept;

  assets::asset_store assets_;
  assets::asset_loader asset_loader_;
  window_system window_system_;
  granit::window window_;
  std::chrono::steady_clock::time_point previous_tick_time_{};
  phase phase_{phase::fresh};
  bool running_{true};
};

} // namespace granit::example

#endif
