// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_tools_core.h"

#include "child_process.h"

#include <spirv_reflect.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <vector>

namespace granit::tools {
namespace {

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

} // namespace

bool inspect_shader(const std::filesystem::path& path, bool emit, shader_info& info,
                    std::ostream& output, std::ostream& error) {
  const auto code = read_file(path);
  if (code.empty() || code.size() % sizeof(std::uint32_t) != 0) {
    error << "无法读取有效 SPIR-V 文件：" << path.string() << '\n';
    return false;
  }
  SpvReflectShaderModule module{};
  if (spvReflectCreateShaderModule(code.size(), code.data(), &module) !=
      SPV_REFLECT_RESULT_SUCCESS) {
    error << "SPIR-V 反射失败：" << path.string() << '\n';
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
    error << "描述符反射失败\n";
    return false;
  }
  std::ranges::sort(bindings, [](const auto* left, const auto* right) {
    return left->set < right->set || (left->set == right->set && left->binding < right->binding);
  });
  if (emit) {
    output << "schema,1\n";
    output << "entry," << info.entry_point << ',' << info.stage << '\n';
    for (const auto* binding : bindings) {
      const auto size = binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                            ? binding->block.padded_size
                            : binding->count;
      output << "binding," << binding->set << ',' << binding->binding << ','
             << binding_type_name(binding->descriptor_type) << ','
             << (binding->name == nullptr ? "" : binding->name) << ',' << size << '\n';
    }
  }
  spvReflectDestroyShaderModule(&module);
  return info.stage != "unsupported" && !info.entry_point.empty();
}

int compile_shader(const compile_options& options, std::ostream& output, std::ostream& error) {
  std::error_code filesystem_error;
  std::filesystem::remove(options.output, filesystem_error);
  process_result process;
  const std::vector<std::string> arguments{
      options.tint.string(),   "--format",          "spirv",
      "--entry-point",         options.entry_point, "--output-name",
      options.output.string(), "--validate",        options.input.string()};
  if (!run_process(arguments, process)) {
    error << "无法启动 Tint：" << options.tint.string() << '\n';
    return 1;
  }
  output << process.standard_output;
  error << process.standard_error;
  if (process.exit_code != 0) {
    std::filesystem::remove(options.output, filesystem_error);
    error << "Tint 编译失败，退出码：" << process.exit_code << '\n';
    return 1;
  }
  shader_info info;
  if (!inspect_shader(options.output, false, info, output, error) ||
      info.entry_point != options.entry_point || info.stage != options.stage) {
    std::filesystem::remove(options.output, filesystem_error);
    error << "Tint 产物的入口点或阶段与请求不一致\n";
    return 1;
  }
  output << "已生成 " << options.output.string() << "（" << info.entry_point << ", " << info.stage
         << "）\n";
  return 0;
}

} // namespace granit::tools
