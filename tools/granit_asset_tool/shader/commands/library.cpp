// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "granit_asset_tool/shader/arguments.h"
#include "granit_asset_tool/shader/commands.h"
#include <granit/asset_tools/asset_tools.hpp>

#include <iostream>

namespace granit::asset_tools::cli {
int build_shader_library(int argc, char** argv) {
  const auto manifest = option_value(argc, argv, "--manifest");
  const auto toolchain = option_value(argc, argv, "--toolchain");
  const auto cache = option_value(argc, argv, "--cache");
  const auto output = option_value(argc, argv, "--output");
  if (!manifest || !toolchain || !cache || !output) {
    std::cerr << "build-library 需要 --manifest、--toolchain、--cache 和 --output\n";
    return 2;
  }
  const granit::asset_tools::shader::source_library_desc desc{
      .manifest_path = *manifest,
      .toolchain_root = *toolchain,
      .cache_path = *cache,
      .output_path = *output,
  };
  const auto [status, cache_hit] = granit::asset_tools::shader::build_library_from_manifest(desc);
  if (status.failed()) {
    std::cerr << "无法从源清单构建 Shader Library：" << *manifest << '\n';
    return 1;
  }
  std::cout << (cache_hit ? "Shader Library 源构建缓存命中：" : "已构建 Shader Library：")
            << *output << '\n';
  return 0;
}

} // namespace granit::asset_tools::cli
