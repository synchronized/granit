// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "process.h"

#include <spirv_reflect.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct shader_info {
  std::string entry_point;
  std::string stage;
};

const char* stage_name(SpvReflectShaderStageFlagBits stage) {
  switch (stage) {
  case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
    return "vertex";
  case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
    return "fragment";
  case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
    return "compute";
  default:
    return "unsupported";
  }
}

const char* binding_type_name(SpvReflectDescriptorType type) {
  switch (type) {
  case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    return "uniform_buffer";
  case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    return "storage_buffer";
  case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    return "sampled_texture";
  case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    return "storage_texture";
  case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
    return "sampler";
  default:
    return "unsupported";
  }
}

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return {};
  const auto end = stream.tellg();
  if (end <= 0)
    return {};
  std::vector<std::byte> data(static_cast<std::size_t>(end));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
  return stream ? data : std::vector<std::byte>{};
}

bool inspect_shader(const std::filesystem::path& path, bool emit, shader_info& info) {
  const auto code = read_file(path);
  if (code.empty() || code.size() % sizeof(std::uint32_t) != 0) {
    std::cerr << "无法读取有效 SPIR-V 文件：" << path.string() << '\n';
    return false;
  }
  SpvReflectShaderModule module{};
  if (spvReflectCreateShaderModule(code.size(), code.data(), &module) !=
      SPV_REFLECT_RESULT_SUCCESS) {
    std::cerr << "SPIR-V 反射失败：" << path.string() << '\n';
    return false;
  }
  info.entry_point = module.entry_point_name == nullptr ? "" : module.entry_point_name;
  info.stage = stage_name(module.shader_stage);
  std::uint32_t binding_count = 0;
  auto reflected = spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
  std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
  if (reflected == SPV_REFLECT_RESULT_SUCCESS)
    reflected = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());
  if (reflected != SPV_REFLECT_RESULT_SUCCESS) {
    spvReflectDestroyShaderModule(&module);
    std::cerr << "描述符反射失败\n";
    return false;
  }
  std::ranges::sort(bindings, [](const auto* left, const auto* right) {
    return left->set < right->set || (left->set == right->set && left->binding < right->binding);
  });
  if (emit) {
    std::cout << "schema,1\n";
    std::cout << "entry," << info.entry_point << ',' << info.stage << '\n';
    for (const auto* binding : bindings) {
      const auto size = binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                            ? binding->block.padded_size
                            : binding->count;
      std::cout << "binding," << binding->set << ',' << binding->binding << ','
                << binding_type_name(binding->descriptor_type) << ','
                << (binding->name == nullptr ? "" : binding->name) << ',' << size << '\n';
    }
  }
  spvReflectDestroyShaderModule(&module);
  return info.stage != "unsupported" && !info.entry_point.empty();
}

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
  std::error_code error;
  std::filesystem::remove(*output, error);
  granit::tools::process_result process;
  const std::vector<std::string> arguments{*tint,           "--format",   "spirv",
                                           "--entry-point", *entry,       "--output-name",
                                           *output,         "--validate", *input};
  if (!granit::tools::run_process(arguments, process)) {
    std::cerr << "无法启动 Tint：" << *tint << '\n';
    return 1;
  }
  std::cout << process.standard_output;
  std::cerr << process.standard_error;
  if (process.exit_code != 0) {
    std::filesystem::remove(*output, error);
    std::cerr << "Tint 编译失败，退出码：" << process.exit_code << '\n';
    return 1;
  }
  shader_info info;
  if (!inspect_shader(*output, false, info) || info.entry_point != *entry || info.stage != *stage) {
    std::filesystem::remove(*output, error);
    std::cerr << "Tint 产物的入口点或阶段与请求不一致\n";
    return 1;
  }
  std::cout << "已生成 " << *output << "（" << info.entry_point << ", " << info.stage << "）\n";
  return 0;
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
  if (argc == 2) {
    std::cerr << "提示：单参数入口已弃用，请改用 inspect 子命令\n";
    shader_info info;
    return inspect_shader(argv[1], true, info) ? 0 : 1;
  }
  if (argc == 3 && std::string_view{argv[1]} == "inspect") {
    shader_info info;
    return inspect_shader(argv[2], true, info) ? 0 : 1;
  }
  if (argc == 3 && std::string_view{argv[1]} == "verify") {
    shader_info info;
    if (!inspect_shader(argv[2], false, info))
      return 1;
    std::cout << "SPIR-V 结构验证通过（" << info.entry_point << ", " << info.stage << "）\n";
    return 0;
  }
  if (argc >= 2 && std::string_view{argv[1]} == "compile")
    return compile_shader(argc, argv);
  print_usage();
  return 2;
}
