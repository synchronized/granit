// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "granit_asset_tool/shader/commands.h"
#include <granit/asset_tools/asset_tools.hpp>

#include <iostream>

namespace granit::asset_tools::cli {
namespace {
const char* object_backend_name(granit::shader_backend backend) {
  return backend == granit::shader_backend::vulkan ? "vulkan" : "webgpu";
}

} // namespace

int print_target_capabilities(granit::shader_backend backend) {
  const auto [status, capabilities] = granit::asset_tools::shader::target_capabilities(backend);
  if (status.failed()) {
    std::cerr << "不支持请求的 Shader 目标档位\n";
    return 1;
  }
  std::cout << "target=" << object_backend_name(capabilities.backend) << "-portable\n"
            << "backend=" << object_backend_name(capabilities.backend) << '\n'
            << "profile=portable\n"
            << "features=none\n";
  return 0;
}

} // namespace granit::asset_tools::cli
