// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/arguments.h"
#include "shader_cli/commands.h"
#include <granit/asset_tools/asset_tools.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::shader_cli {
namespace {
bool parse_defines(const std::vector<std::string>& arguments,
                   std::vector<std::pair<std::string, std::string>>& output) {
  output.clear();
  output.reserve(arguments.size());
  for (const auto& argument : arguments) {
    const auto separator = argument.find('=');
    if (separator == std::string::npos || separator == 0 || separator + 1 == argument.size())
      return false;
    output.emplace_back(argument.substr(0, separator), argument.substr(separator + 1));
  }
  std::ranges::sort(output);
  return std::ranges::adjacent_find(output, [](const auto& left, const auto& right) {
           return left.first == right.first;
         }) == output.end();
}

} // namespace

int compile_shader(int argc, char** argv) {
  const auto toolchain = option_value(argc, argv, "--toolchain");
  const auto input = option_value(argc, argv, "--input");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto spirv_output = option_value(argc, argv, "--spirv-output");
  const auto wgsl_output = option_value(argc, argv, "--wgsl-output");
  std::vector<std::pair<std::string, std::string>> definitions;
  const auto define_arguments = option_values(argc, argv, "--define");
  if (!toolchain || !input || !entry || !stage || !spirv_output || !wgsl_output ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute") ||
      !parse_defines(define_arguments, definitions)) {
    std::cerr << "compile 需要 --toolchain、--input、--entry、--stage、"
                 "--spirv-output 和 --wgsl-output\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? GRANIT_SHADER_STAGE_VERTEX
                           : *stage == "fragment" ? GRANIT_SHADER_STAGE_FRAGMENT
                                                  : GRANIT_SHADER_STAGE_COMPUTE;
  std::vector<granit::asset_tools::shader::shader_define> shader_definitions;
  shader_definitions.reserve(definitions.size());
  for (const auto& [name, value] : definitions) {
    shader_definitions.push_back({.name = name, .value = value});
  }
  granit::asset_tools::shader::compiler compiler;
  if (compiler.initialize({*toolchain}).failed()) {
    std::cerr << "Shader Compiler 创建失败\n";
    return 1;
  }
  granit::asset_tools::shader::compile_desc desc;
  desc.input_path = *input;
  desc.stage = static_cast<granit::shader_stage>(stage_value);
  desc.entry_point = *entry;
  desc.target_backends = granit::shader_backend::all;
  desc.spirv_output_path = *spirv_output;
  desc.wgsl_output_path = *wgsl_output;
  desc.defines = shader_definitions;
  auto [status, result] = compiler.compile(desc);
  const auto info = result.info();
  std::cout << info.output;
  std::cerr << info.diagnostic;
  return status.ok() ? 0 : 1;
}

} // namespace granit::shader_cli
