// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/commands.h"
#include <granit/tools/shader_tools.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace granit::shader_cli {
namespace {
std::string json_string(std::string_view value) {
  std::ostringstream output;
  output << '"';
  for (const auto character : value) {
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20) {
        constexpr char hex[] = "0123456789abcdef";
        output << "\\u00" << hex[(static_cast<unsigned char>(character) >> 4) & 0x0f]
               << hex[static_cast<unsigned char>(character) & 0x0f];
      } else {
        output << character;
      }
      break;
    }
  }
  output << '"';
  return std::move(output).str();
}

const char* binding_type_name(granit::shader_tools::binding_type type) {
  switch (type) {
  case granit::shader_tools::binding_type::uniform_buffer:
    return "uniform_buffer";
  case granit::shader_tools::binding_type::storage_buffer:
    return "storage_buffer";
  case granit::shader_tools::binding_type::sampled_texture:
    return "sampled_texture";
  case granit::shader_tools::binding_type::storage_texture:
    return "storage_texture";
  case granit::shader_tools::binding_type::sampler:
    return "sampler";
  default:
    return "unsupported";
  }
}

const char* binding_access_name(granit::shader_tools::binding_access access) {
  switch (access) {
  case granit::shader_tools::binding_access::read:
    return "read";
  case granit::shader_tools::binding_access::write:
    return "write";
  case granit::shader_tools::binding_access::read_write:
    return "read_write";
  default:
    return "unsupported";
  }
}

const char* scalar_type_name(granit::shader_tools::scalar_type type) {
  switch (type) {
  case granit::shader_tools::scalar_type::floating_point:
    return "float";
  case granit::shader_tools::scalar_type::signed_integer:
    return "sint";
  case granit::shader_tools::scalar_type::unsigned_integer:
    return "uint";
  default:
    return "unsupported";
  }
}

void print_interface_variable(const granit::shader_tools::interface_variable_info& variable) {
  std::cout << "{\"location\": " << variable.location << ", \"component\": " << variable.component
            << ", \"scalar_type\": " << json_string(scalar_type_name(variable.scalar_type))
            << ", \"bit_width\": " << variable.bit_width
            << ", \"vector_size\": " << variable.vector_size
            << ", \"name\": " << json_string(variable.name) << '}';
}

void print_json(const granit::shader_tools::reflection& result,
                const granit::shader_tools::reflection_info& info, const char* stage) {
  std::cout << "{\n  \"schema\": 1,\n  \"entry_point\": " << json_string(info.entry_point)
            << ",\n  \"stage\": " << json_string(stage) << ",\n  \"bindings\": [";
  for (uint64_t index = 0; index < result.binding_count(); ++index) {
    const auto [status, binding] = result.binding(index);
    if (status.failed())
      continue;
    std::cout << (index == 0 ? "\n" : ",\n") << "    {\"group\": " << binding.group
              << ", \"binding\": " << binding.binding
              << ", \"type\": " << json_string(binding_type_name(binding.type))
              << ", \"access\": " << json_string(binding_access_name(binding.access))
              << ", \"name\": " << json_string(binding.name)
              << ", \"array_count\": " << binding.array_count
              << ", \"minimum_binding_size\": " << binding.minimum_binding_size << '}';
  }
  std::cout << (result.binding_count() == 0 ? "" : "\n") << "  ],\n  \"vertex_inputs\": [";
  for (uint64_t index = 0; index < result.vertex_input_count(); ++index) {
    const auto [status, input] = result.vertex_input(index);
    if (status.failed())
      continue;
    std::cout << (index == 0 ? "\n    " : ",\n    ");
    print_interface_variable(input);
  }
  std::cout << (result.vertex_input_count() == 0 ? "" : "\n") << "  ],\n  \"fragment_outputs\": [";
  for (uint64_t index = 0; index < result.fragment_output_count(); ++index) {
    const auto [status, output] = result.fragment_output(index);
    if (status.failed())
      continue;
    std::cout << (index == 0 ? "\n    " : ",\n    ");
    print_interface_variable(output);
  }
  const auto workgroup = result.compute_workgroup_size();
  std::cout << (result.fragment_output_count() == 0 ? "" : "\n")
            << "  ],\n  \"workgroup_size\": {\"x\": " << workgroup.x << ", \"y\": " << workgroup.y
            << ", \"z\": " << workgroup.z << "},\n  \"overrides\": [";
  for (uint64_t index = 0; index < result.override_count(); ++index) {
    const auto [status, override_info] = result.override_at(index);
    if (status.failed())
      continue;
    std::cout << (index == 0 ? "\n" : ",\n") << "    {\"id\": " << override_info.id
              << ", \"scalar_type\": " << json_string(scalar_type_name(override_info.scalar_type))
              << ", \"bit_width\": " << override_info.bit_width
              << ", \"name\": " << json_string(override_info.name)
              << ", \"default_value\": " << override_info.default_value
              << ", \"default_value_size\": " << override_info.default_value_size << '}';
  }
  std::cout << (result.override_count() == 0 ? "" : "\n") << "  ]\n}\n";
}

} // namespace

int inspect_shader(const char* path, bool verify, bool json) {
  granit_shader_tools_inspect_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.input_path = path;
  desc.input_path_length = std::char_traits<char>::length(path);
  auto [status, result] = granit::shader_tools::inspect_spirv(desc);
  const auto info = result.info();
  const auto stage = info.stage == granit::shader_stage::vertex     ? "vertex"
                     : info.stage == granit::shader_stage::fragment ? "fragment"
                     : info.stage == granit::shader_stage::compute  ? "compute"
                                                                    : "unsupported";
  if (json && status.ok())
    print_json(result, info, stage);
  else if (verify && status.ok())
    std::cout << "SPIR-V 结构验证通过（" << info.entry_point << ", " << stage << "）\n";
  else
    std::cout << info.output;
  std::cerr << info.diagnostic;
  return status.ok() ? 0 : 1;
}

} // namespace granit::shader_cli
