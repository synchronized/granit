// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.hpp>

#include <iostream>
#include <optional>
#include <sstream>
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

const char* binding_type_name(uint32_t type) {
  switch (type) {
  case GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER:
    return "uniform_buffer";
  case GRANIT_SHADER_TOOLS_BINDING_STORAGE_BUFFER:
    return "storage_buffer";
  case GRANIT_SHADER_TOOLS_BINDING_SAMPLED_TEXTURE:
    return "sampled_texture";
  case GRANIT_SHADER_TOOLS_BINDING_STORAGE_TEXTURE:
    return "storage_texture";
  case GRANIT_SHADER_TOOLS_BINDING_SAMPLER:
    return "sampler";
  default:
    return "unsupported";
  }
}

const char* binding_access_name(uint32_t access) {
  switch (access) {
  case GRANIT_SHADER_TOOLS_ACCESS_READ:
    return "read";
  case GRANIT_SHADER_TOOLS_ACCESS_WRITE:
    return "write";
  case GRANIT_SHADER_TOOLS_ACCESS_READ_WRITE:
    return "read_write";
  default:
    return "unsupported";
  }
}

const char* scalar_type_name(uint32_t type) {
  switch (type) {
  case GRANIT_SHADER_TOOLS_SCALAR_FLOAT:
    return "float";
  case GRANIT_SHADER_TOOLS_SCALAR_SINT:
    return "sint";
  case GRANIT_SHADER_TOOLS_SCALAR_UINT:
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

void print_json(const granit::shader_tools::result& result,
                const granit::shader_tools::result_info& info, const char* stage) {
  std::cout << "{\n  \"schema\": 1,\n  \"entry_point\": " << json_string(info.entry_point)
            << ",\n  \"stage\": " << json_string(stage) << ",\n  \"bindings\": [";
  for (uint64_t index = 0; index < result.binding_count(); ++index) {
    const auto [status, binding] = result.binding(index);
    if (status != GRANIT_SUCCESS)
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
    if (status != GRANIT_SUCCESS)
      continue;
    std::cout << (index == 0 ? "\n    " : ",\n    ");
    print_interface_variable(input);
  }
  std::cout << (result.vertex_input_count() == 0 ? "" : "\n") << "  ],\n  \"fragment_outputs\": [";
  for (uint64_t index = 0; index < result.fragment_output_count(); ++index) {
    const auto [status, output] = result.fragment_output(index);
    if (status != GRANIT_SUCCESS)
      continue;
    std::cout << (index == 0 ? "\n    " : ",\n    ");
    print_interface_variable(output);
  }
  const auto workgroup = result.compute_workgroup_size();
  std::cout << (result.fragment_output_count() == 0 ? "" : "\n")
            << "  ],\n  \"workgroup_size\": {\"x\": " << workgroup.x << ", \"y\": " << workgroup.y
            << ", \"z\": " << workgroup.z << "}\n}\n";
}

int inspect_shader(const char* path, bool verify, bool json = false) {
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
  if (json && status == GRANIT_SUCCESS)
    print_json(result, info, stage);
  else if (verify && status == GRANIT_SUCCESS)
    std::cout << "SPIR-V 结构验证通过（" << info.entry_point << ", " << stage << "）\n";
  else
    std::cout << info.output;
  std::cerr << info.diagnostic;
  return status == GRANIT_SUCCESS ? 0 : 1;
}

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_shader_tool inspect <shader.spv>\n"
               "  granit_shader_tool inspect --json <shader.spv>\n"
               "  granit_shader_tool verify <shader.spv>\n"
               "  granit_shader_tool compile --tint <path> --input <shader.wgsl> "
               "--entry <name> --stage <vertex|fragment|compute> --output <shader.spv>\n";
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string_view{argv[1]} == "inspect") {
    return inspect_shader(argv[2], false);
  }
  if (argc == 4 && std::string_view{argv[1]} == "inspect" && std::string_view{argv[2]} == "--json")
    return inspect_shader(argv[3], false, true);
  if (argc == 3 && std::string_view{argv[1]} == "verify") {
    return inspect_shader(argv[2], true);
  }
  if (argc >= 2 && std::string_view{argv[1]} == "compile")
    return compile_shader(argc, argv);
  print_usage();
  return 2;
}
