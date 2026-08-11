// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <spirv_reflect.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

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

std::vector<std::byte> read_file(const char* path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) {
    return {};
  }
  const auto end = stream.tellg();
  if (end <= 0) {
    return {};
  }
  std::vector<std::byte> data(static_cast<std::size_t>(end));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
  return stream ? data : std::vector<std::byte>{};
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "用法：granit_shader_tool <shader.spv>\n";
    return 2;
  }
  const auto code = read_file(argv[1]);
  if (code.empty() || code.size() % sizeof(std::uint32_t) != 0) {
    std::cerr << "无法读取有效 SPIR-V 文件\n";
    return 1;
  }

  SpvReflectShaderModule module{};
  if (spvReflectCreateShaderModule(code.size(), code.data(), &module) !=
      SPV_REFLECT_RESULT_SUCCESS) {
    std::cerr << "SPIR-V 反射失败\n";
    return 1;
  }

  std::uint32_t binding_count = 0;
  auto reflected = spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
  std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
  if (reflected == SPV_REFLECT_RESULT_SUCCESS) {
    reflected = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());
  }
  if (reflected != SPV_REFLECT_RESULT_SUCCESS) {
    spvReflectDestroyShaderModule(&module);
    std::cerr << "描述符反射失败\n";
    return 1;
  }
  std::ranges::sort(bindings, [](const auto* left, const auto* right) {
    return left->set < right->set || (left->set == right->set && left->binding < right->binding);
  });

  std::cout << "schema,1\n";
  std::cout << "entry," << module.entry_point_name << ',' << stage_name(module.shader_stage)
            << '\n';
  for (const auto* binding : bindings) {
    const auto size = binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                          ? binding->block.padded_size
                          : binding->count;
    std::cout << "binding," << binding->set << ',' << binding->binding << ','
              << binding_type_name(binding->descriptor_type) << ','
              << (binding->name == nullptr ? "" : binding->name) << ',' << size << '\n';
  }
  spvReflectDestroyShaderModule(&module);
  return 0;
}
