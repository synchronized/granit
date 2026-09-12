// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/arguments.h"
#include "shader_cli/commands.h"
#include <granit/tools/shader_tools.hpp>

#include <iostream>
#include <string_view>

namespace granit::shader_cli {

int build_shader_object(int argc, char** argv) {
  const auto spirv_path = option_value(argc, argv, "--spirv");
  const auto wgsl_path = option_value(argc, argv, "--wgsl");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto object_path = option_value(argc, argv, "--output");
  if (!spirv_path || !wgsl_path || !entry || !stage || !object_path ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute")) {
    std::cerr << "object 需要 --spirv、--wgsl、--entry、--stage 和 --output\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? granit::shader_stage::vertex
                           : *stage == "fragment" ? granit::shader_stage::fragment
                                                  : granit::shader_stage::compute;
  constexpr std::string_view identity = "import-v1";
  constexpr std::string_view target = "portable";
  constexpr std::string_view options = "validated-pair";
  const granit_shader_tools_object_desc desc{
      .struct_size = sizeof(granit_shader_tools_object_desc),
      .source_path = wgsl_path->data(),
      .source_path_length = wgsl_path->size(),
      .wgsl_path = wgsl_path->data(),
      .wgsl_path_length = wgsl_path->size(),
      .spirv_path = spirv_path->data(),
      .spirv_path_length = spirv_path->size(),
      .output_path = object_path->data(),
      .output_path_length = object_path->size(),
      .tint_revision = identity.data(),
      .tint_revision_length = identity.size(),
      .target_environment = target.data(),
      .target_environment_length = target.size(),
      .compile_options = options.data(),
      .compile_options_length = options.size(),
      .backend_mask = GRANIT_SHADER_BACKEND_ALL_BITS,
      .required_features = 0,
      .entry_point = entry->data(),
      .entry_point_length = entry->size(),
      .stage = static_cast<granit_shader_stage>(stage_value),
      .reserved = 0,
  };
  const auto [status, cache_hit] = granit::shader_tools::build_object(desc);
  if (status.failed()) {
    std::cerr << "无法构建 Shader Object\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader Object 未变化：" : "已构建 Shader Object：") << *object_path
            << '\n';
  return 0;
}

} // namespace granit::shader_cli
