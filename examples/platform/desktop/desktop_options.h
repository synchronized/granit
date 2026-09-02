// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PLATFORM_DESKTOP_DESKTOP_OPTIONS_H_
#define GRANIT_EXAMPLES_PLATFORM_DESKTOP_DESKTOP_OPTIONS_H_

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/swapchain.hpp>

#include <span>
#include <string>
#include <string_view>

namespace granit::example::model_viewer::desktop {

struct options {
  granit::renderer_backend backend{granit::renderer_backend::automatic};
  std::string backend_library_path;
  std::string asset_path;
  std::string profile_output_path;
  granit::present_mode presentation{granit::present_mode::mailbox};
  bool enable_validation{};
  bool smoke_test{};
  bool show_ui{true};
};

/**
 * 解析桌面查看器参数。参数错误时返回 invalid_argument，并保留 output 原值。
 * 支持后端、资产、验证、Smoke、UI、呈现模式和固定性能采样参数。
 */
[[nodiscard]] granit::result parse_options(std::span<const std::string_view> arguments,
                                           options& output);

} // namespace granit::example::model_viewer::desktop

#endif
