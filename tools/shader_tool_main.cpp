// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.hpp>

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
  const auto stage_value = *stage == "vertex"     ? GRANIT_SHADER_TOOLS_STAGE_VERTEX
                           : *stage == "fragment" ? GRANIT_SHADER_TOOLS_STAGE_FRAGMENT
                                                  : GRANIT_SHADER_TOOLS_STAGE_COMPUTE;
  granit_shader_tools_compile_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.tint_path = tint->data();
  desc.tint_path_length = tint->size();
  desc.input_path = input->data();
  desc.input_path_length = input->size();
  desc.entry_point = entry->data();
  desc.entry_point_length = entry->size();
  desc.stage = stage_value;
  desc.output_path = output->data();
  desc.output_path_length = output->size();
  auto [status, result] = granit::shader_tools::compile_wgsl(desc);
  const auto info = result.info();
  std::cout << info.output;
  std::cerr << info.diagnostic;
  return status == GRANIT_SUCCESS ? 0 : 1;
}

int inspect_shader(const char* path, bool verify) {
  granit_shader_tools_inspect_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.input_path = path;
  desc.input_path_length = std::char_traits<char>::length(path);
  auto [status, result] = granit::shader_tools::inspect_spirv(desc);
  const auto info = result.info();
  const auto stage = info.stage == GRANIT_SHADER_TOOLS_STAGE_VERTEX     ? "vertex"
                     : info.stage == GRANIT_SHADER_TOOLS_STAGE_FRAGMENT ? "fragment"
                     : info.stage == GRANIT_SHADER_TOOLS_STAGE_COMPUTE  ? "compute"
                                                                        : "unsupported";
  if (verify && status == GRANIT_SUCCESS)
    std::cout << "SPIR-V 结构验证通过（" << info.entry_point << ", " << stage << "）\n";
  else
    std::cout << info.output;
  std::cerr << info.diagnostic;
  return status == GRANIT_SUCCESS ? 0 : 1;
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
    return inspect_shader(argv[2], false);
  }
  if (argc == 3 && std::string_view{argv[1]} == "verify") {
    return inspect_shader(argv[2], true);
  }
  if (argc >= 2 && std::string_view{argv[1]} == "compile")
    return compile_shader(argc, argv);
  print_usage();
  return 2;
}
