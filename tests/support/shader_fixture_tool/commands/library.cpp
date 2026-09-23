// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_tools/shader/library_builder.h"
#include "granit_asset_tool/shader/arguments.h"
#include "shader_fixture_tool/commands.h"

#include <filesystem>
#include <iostream>
#include <vector>

namespace granit::asset_tools::cli {
int link_shader_fixture_library(int argc, char** argv) {
  const auto object_specs = option_values(argc, argv, "--object");
  const auto library_name = option_value(argc, argv, "--name");
  const auto target = option_value(argc, argv, "--target");
  const auto output_path = option_value(argc, argv, "--output");
  if (object_specs.empty() || !library_name || library_name->empty() || !target || !output_path ||
      (*target != "all" && *target != "vulkan" && *target != "webgpu")) {
    std::cerr << "library 需要 --name、一个或多个 --object <逻辑名称=路径>、"
                 "--target <all|vulkan|webgpu> 和 --output\n";
    return 2;
  }
  const auto backend_mask = *target == "all"      ? GRANIT_SHADER_BACKEND_ALL_BITS
                            : *target == "vulkan" ? GRANIT_SHADER_BACKEND_VULKAN_BIT
                                                  : GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  std::vector<std::filesystem::path> objects;
  std::vector<std::string> logical_names;
  objects.reserve(object_specs.size());
  logical_names.reserve(object_specs.size());
  for (const auto& spec : object_specs) {
    const auto separator = spec.find('=');
    if (separator == std::string::npos || separator == 0 || separator + 1 == spec.size()) {
      std::cerr << "无效的 Shader Object 映射：" << spec << '\n';
      return 2;
    }
    logical_names.push_back(spec.substr(0, separator));
    objects.emplace_back(spec.substr(separator + 1));
  }
  bool cache_hit = false;
  if (granit::asset_tools::detail::link_shader_library(objects, logical_names, *library_name,
                                                       backend_mask, *output_path,
                                                       cache_hit) != GRANIT_SUCCESS) {
    std::cerr << "无法链接 Shader Library\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader Library 内容未变化：" : "已链接 Shader Library：")
            << *output_path << '\n';
  return 0;
}

} // namespace granit::asset_tools::cli
