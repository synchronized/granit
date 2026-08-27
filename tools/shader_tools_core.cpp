// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_tools_core.h"

#include "child_process.h"

#include <spirv_reflect.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ostream>
#include <sstream>
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

const char* binding_access_name(shader_binding_access access) {
  switch (access) {
  case shader_binding_access::read:
    return "read";
  case shader_binding_access::write:
    return "write";
  case shader_binding_access::read_write:
    return "read_write";
  }
  return "unsupported";
}

const char* scalar_type_name(shader_scalar_type type) {
  switch (type) {
  case shader_scalar_type::floating_point:
    return "float";
  case shader_scalar_type::signed_integer:
    return "sint";
  case shader_scalar_type::unsigned_integer:
    return "uint";
  }
  return "unsupported";
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
    }
  }
  output << '"';
  return std::move(output).str();
}

void write_interface_json(std::ostream& output, const shader_interface_variable_info& value) {
  output << "{\"location\": " << value.location << ", \"component\": " << value.component
         << ", \"scalar_type\": " << json_string(scalar_type_name(value.scalar_type))
         << ", \"bit_width\": " << value.bit_width << ", \"vector_size\": " << value.vector_size
         << ", \"name\": " << json_string(value.name) << '}';
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

bool reflect_interface_variable(const SpvReflectInterfaceVariable& source,
                                shader_interface_variable_info& target) {
  if ((source.decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) != 0)
    return false;
  target.location = source.location;
  target.component = source.component == UINT32_MAX ? 0 : source.component;
  target.name = source.name == nullptr ? "" : source.name;
  target.bit_width = source.numeric.scalar.width;
  target.vector_size = std::max(source.numeric.vector.component_count, UINT32_C(1));
  const SpvReflectTypeFlags flags =
      source.type_description == nullptr
          ? static_cast<SpvReflectTypeFlags>(SPV_REFLECT_TYPE_FLAG_UNDEFINED)
          : source.type_description->type_flags;
  if ((flags & SPV_REFLECT_TYPE_FLAG_FLOAT) != 0) {
    target.scalar_type = shader_scalar_type::floating_point;
  } else if ((flags & SPV_REFLECT_TYPE_FLAG_INT) != 0) {
    target.scalar_type = source.numeric.scalar.signedness != 0
                             ? shader_scalar_type::signed_integer
                             : shader_scalar_type::unsigned_integer;
  } else {
    return false;
  }
  return target.bit_width != 0;
}

void collect_interface_variable(const SpvReflectInterfaceVariable& variable,
                                std::vector<shader_interface_variable_info>& output) {
  if (variable.member_count != 0) {
    for (std::uint32_t member = 0; member < variable.member_count; ++member)
      collect_interface_variable(variable.members[member], output);
  } else {
    shader_interface_variable_info reflected;
    if (reflect_interface_variable(variable, reflected))
      output.push_back(std::move(reflected));
  }
}

void collect_interface_variables(SpvReflectInterfaceVariable* const* variables, std::uint32_t count,
                                 std::vector<shader_interface_variable_info>& output) {
  output.clear();
  for (std::uint32_t index = 0; index < count; ++index)
    collect_interface_variable(*variables[index], output);
  std::ranges::sort(output, [](const auto& left, const auto& right) {
    return left.location < right.location ||
           (left.location == right.location && left.component < right.component);
  });
}

bool reflect_scalar_type(const SpvReflectTypeDescription* description, shader_scalar_type& type) {
  if (description == nullptr)
    return false;
  if ((description->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) != 0) {
    type = shader_scalar_type::floating_point;
    return true;
  }
  if ((description->type_flags & SPV_REFLECT_TYPE_FLAG_INT) != 0) {
    type = description->traits.numeric.scalar.signedness != 0
               ? shader_scalar_type::signed_integer
               : shader_scalar_type::unsigned_integer;
    return true;
  }
  if ((description->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL) != 0) {
    type = shader_scalar_type::unsigned_integer;
    return true;
  }
  return false;
}

bool collect_overrides(const SpvReflectShaderModule& module,
                       std::vector<shader_override_info>& output) {
  std::uint32_t count = 0;
  auto reflected = spvReflectEnumerateSpecializationConstants(&module, &count, nullptr);
  std::vector<SpvReflectSpecializationConstant*> constants(count);
  if (reflected == SPV_REFLECT_RESULT_SUCCESS)
    reflected = spvReflectEnumerateSpecializationConstants(&module, &count, constants.data());
  if (reflected != SPV_REFLECT_RESULT_SUCCESS)
    return false;
  output.clear();
  output.reserve(constants.size());
  for (const auto* constant : constants) {
    shader_override_info value;
    value.id = constant->constant_id;
    value.name = constant->name == nullptr ? "" : constant->name;
    value.bit_width = constant->type_description == nullptr
                          ? 0
                          : constant->type_description->traits.numeric.scalar.width;
    if (constant->type_description != nullptr &&
        (constant->type_description->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL) != 0)
      value.bit_width = 1;
    if (!reflect_scalar_type(constant->type_description, value.scalar_type) ||
        constant->default_value_size > sizeof(value.default_value))
      return false;
    value.default_value_size = constant->default_value_size;
    if (value.default_value_size != 0 && constant->default_value != nullptr)
      std::memcpy(&value.default_value, constant->default_value, value.default_value_size);
    output.push_back(std::move(value));
  }
  std::ranges::sort(output, [](const auto& left, const auto& right) { return left.id < right.id; });
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

std::string serialize_shader_info_json(const shader_info& info) {
  std::ostringstream output;
  output << "{\n  \"schema\": 1,\n  \"entry_point\": " << json_string(info.entry_point)
         << ",\n  \"stage\": " << json_string(info.stage) << ",\n  \"bindings\": [";
  for (std::size_t index = 0; index < info.bindings.size(); ++index) {
    const auto& binding = info.bindings[index];
    output << (index == 0 ? "\n" : ",\n") << "    {\"group\": " << binding.group
           << ", \"binding\": " << binding.binding
           << ", \"type\": " << json_string(binding_type_name(binding.type))
           << ", \"access\": " << json_string(binding_access_name(binding.access))
           << ", \"name\": " << json_string(binding.name)
           << ", \"array_count\": " << binding.array_count
           << ", \"minimum_binding_size\": " << binding.minimum_binding_size << '}';
  }
  output << (info.bindings.empty() ? "" : "\n") << "  ],\n  \"vertex_inputs\": [";
  for (std::size_t index = 0; index < info.vertex_inputs.size(); ++index) {
    output << (index == 0 ? "\n    " : ",\n    ");
    write_interface_json(output, info.vertex_inputs[index]);
  }
  output << (info.vertex_inputs.empty() ? "" : "\n") << "  ],\n  \"fragment_outputs\": [";
  for (std::size_t index = 0; index < info.fragment_outputs.size(); ++index) {
    output << (index == 0 ? "\n    " : ",\n    ");
    write_interface_json(output, info.fragment_outputs[index]);
  }
  output << (info.fragment_outputs.empty() ? "" : "\n")
         << "  ],\n  \"workgroup_size\": {\"x\": " << info.workgroup_size_x
         << ", \"y\": " << info.workgroup_size_y << ", \"z\": " << info.workgroup_size_z
         << "},\n  \"overrides\": [";
  for (std::size_t index = 0; index < info.overrides.size(); ++index) {
    const auto& value = info.overrides[index];
    output << (index == 0 ? "\n" : ",\n") << "    {\"id\": " << value.id
           << ", \"scalar_type\": " << json_string(scalar_type_name(value.scalar_type))
           << ", \"bit_width\": " << value.bit_width << ", \"name\": " << json_string(value.name)
           << ", \"default_value\": " << value.default_value
           << ", \"default_value_size\": " << value.default_value_size << '}';
  }
  output << (info.overrides.empty() ? "" : "\n") << "  ]\n}\n";
  return std::move(output).str();
}

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
  const auto* entry_point = spvReflectGetEntryPoint(&module, info.entry_point.c_str());
  if (entry_point == nullptr) {
    spvReflectDestroyShaderModule(&module);
    error << "无法读取 SPIR-V 入口点\n";
    return false;
  }
  if (info.stage == "vertex")
    collect_interface_variables(entry_point->input_variables, entry_point->input_variable_count,
                                info.vertex_inputs);
  if (info.stage == "fragment")
    collect_interface_variables(entry_point->output_variables, entry_point->output_variable_count,
                                info.fragment_outputs);
  if (info.stage == "compute") {
    info.workgroup_size_x = entry_point->local_size.x;
    info.workgroup_size_y = entry_point->local_size.y;
    info.workgroup_size_z = entry_point->local_size.z;
  }
  if (!collect_overrides(module, info.overrides)) {
    spvReflectDestroyShaderModule(&module);
    error << "Override 常量反射失败\n";
    return false;
  }
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
    error << "Tint 编译失败，入口点：" << options.entry_point << "，阶段：" << options.stage
          << "，退出码：" << process.exit_code << '\n';
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
