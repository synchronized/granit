// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PLATFORM_DESKTOP_DESKTOP_OPTIONS_H_
#define GRANIT_EXAMPLES_PLATFORM_DESKTOP_DESKTOP_OPTIONS_H_

#include <granit/renderer/renderer.hpp>

#include <span>
#include <string>
#include <string_view>

namespace granit::example::model_viewer::desktop {

struct options {
  granit::renderer_backend backend{granit::renderer_backend::automatic};
  std::string backend_library_path;
  std::string asset_path;
  bool enable_validation{};
  bool smoke_test{};
};

/**
 * 解析桌面查看器参数。参数错误时返回 invalid_argument，并保留 output 原值。
 * 支持 --backend=auto|vulkan|webgpu、--backend-library、--asset、--validation 和
 * --smoke-test。
 */
[[nodiscard]] granit::result parse_options(std::span<const std::string_view> arguments,
                                           options& output);

} // namespace granit::example::model_viewer::desktop

#endif
