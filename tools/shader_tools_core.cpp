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

const char* binding_type_name(shader_binding_type type) {
  switch (type) {
  case shader_binding_type::uniform_buffer:
    return "uniform_buffer";
  case shader_binding_type::storage_buffer:
    return "storage_buffer";
  case shader_binding_type::sampled_texture:
    return "sampled_texture";
  case shader_binding_type::storage_texture:
    return "storage_texture";
  case shader_binding_type::sampler:
    return "sampler";
  }
  return "unsupported";
}

bool reflect_binding(const SpvReflectDescriptorBinding& source, shader_binding_info& target) {
  target.group = source.set;
  target.binding = source.binding;
  target.name = source.name == nullptr ? "" : source.name;
  target.array_count = source.count;
  target.access = shader_binding_access::read;
  switch (source.descriptor_type) {
  case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    target.type = shader_binding_type::uniform_buffer;
    target.minimum_binding_size = source.block.padded_size;
    return true;
  case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    target.type = shader_binding_type::storage_buffer;
    target.minimum_binding_size = source.block.padded_size;
    break;
  case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    target.type = shader_binding_type::sampled_texture;
    return true;
  case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    target.type = shader_binding_type::storage_texture;
    break;
  case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
    target.type = shader_binding_type::sampler;
    return true;
  default:
    return false;
  }
  const auto non_writable = (source.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE) != 0;
  const auto non_readable = (source.decoration_flags & SPV_REFLECT_DECORATION_NON_READABLE) != 0;
  target.access = non_writable   ? shader_binding_access::read
                  : non_readable ? shader_binding_access::write
                                 : shader_binding_access::read_write;
  return true;
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
  info.bindings.clear();
  info.bindings.reserve(bindings.size());
  for (const auto* binding : bindings) {
    shader_binding_info reflected_binding;
    if (!reflect_binding(*binding, reflected_binding)) {
      spvReflectDestroyShaderModule(&module);
      error << "不支持的描述符类型：group=" << binding->set << " binding=" << binding->binding
            << '\n';
      return false;
    }
    info.bindings.push_back(std::move(reflected_binding));
  }
  if (emit) {
    output << "schema,1\n";
    output << "entry," << info.entry_point << ',' << info.stage << '\n';
    for (const auto& binding : info.bindings) {
      const auto size =
          binding.minimum_binding_size != 0 ? binding.minimum_binding_size : binding.array_count;
      output << "binding," << binding.group << ',' << binding.binding << ','
             << binding_type_name(binding.type) << ',' << binding.name << ',' << size << '\n';
    }
  }
  spvReflectDestroyShaderModule(&module);
  return info.stage != "unsupported" && !info.entry_point.empty();
}

int compile_shader(const compile_options& options, shader_info& info, std::ostream& output,
                   std::ostream& error) {
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
