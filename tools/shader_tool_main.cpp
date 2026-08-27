// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_tools_core.h"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

std::optional<std::string> option_value(int argc, char** argv, std::string_view name) {
  for (int index = 2; index + 1 < argc; ++index) {
    if (argv[index] == name)
      return argv[index + 1];
  }
  return std::nullopt;
}

int compile_shader(int argc, char** argv) {
  const auto tint = option_value(argc, argv, "--tint");
  const auto input = option_value(argc, argv, "--input");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto output = option_value(argc, argv, "--output");
  if (!tint || !input || !entry || !stage || !output ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute")) {
    std::cerr << "compile 需要 --tint、--input、--entry、--stage 和 --output\n";
    return 2;
  }
  return granit::tools::compile_shader({*tint, *input, *entry, *stage, *output}, std::cout,
                                       std::cerr);
}

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_shader_tool inspect <shader.spv>\n"
               "  granit_shader_tool verify <shader.spv>\n"
               "  granit_shader_tool compile --tint <path> --input <shader.wgsl> "
               "--entry <name> --stage <vertex|fragment|compute> --output <shader.spv>\n";
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string_view{argv[1]} == "inspect") {
    granit::tools::shader_info info;
    return granit::tools::inspect_shader(argv[2], true, info, std::cout, std::cerr) ? 0 : 1;
  }
  if (argc == 3 && std::string_view{argv[1]} == "verify") {
    granit::tools::shader_info info;
    if (!granit::tools::inspect_shader(argv[2], false, info, std::cout, std::cerr))
      return 1;
    std::cout << "SPIR-V 结构验证通过（" << info.entry_point << ", " << info.stage << "）\n";
    return 0;
  }
  if (argc >= 2 && std::string_view{argv[1]} == "compile")
    return compile_shader(argc, argv);
  print_usage();
  return 2;
}
