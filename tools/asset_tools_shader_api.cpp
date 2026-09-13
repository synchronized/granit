// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/asset_tools.h>

#include "asset_tools_shader_core.h"
#include "shader_toolchain_layout.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct stored_compiler {
  std::filesystem::path dxc;
  std::filesystem::path tint;
};
struct stored_shader_data {
  granit_result status = GRANIT_ERROR_INTERNAL;
  std::string entry_point;
  uint32_t stage = 0;
  std::string output;
  std::string diagnostic;
  std::vector<std::byte> spirv;
  std::string wgsl;
  std::string reflection_json;
  std::vector<granit::tools::shader_binding_info> bindings;
  std::vector<granit::tools::shader_interface_variable_info> vertex_inputs;
  std::vector<granit::tools::shader_interface_variable_info> fragment_outputs;
  std::vector<granit::tools::shader_override_info> overrides;
  uint32_t workgroup_size_x = 0;
  uint32_t workgroup_size_y = 0;
  uint32_t workgroup_size_z = 0;
};

std::mutex shader_data_mutex;
std::unordered_map<uint64_t, std::shared_ptr<const stored_shader_data>> compilations;
std::unordered_map<uint64_t, std::shared_ptr<const stored_shader_data>> reflections;
std::atomic<uint64_t> next_shader_data_handle{1};
std::mutex compilers_mutex;
std::unordered_map<uint64_t, std::shared_ptr<const stored_compiler>> compilers;
std::atomic<uint64_t> next_compiler{1};

bool valid_string(const char* value, uint64_t length) { return value != nullptr || length == 0; }

const char* stage_name(uint32_t stage);

bool valid_define_name(std::string_view name) {
  if (name.empty() ||
      !(std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_'))
    return false;
  return std::ranges::all_of(name.substr(1), [](char value) {
    return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
  });
}

std::string copy_string(const char* value, uint64_t length) {
  return length == 0 ? std::string{} : std::string{value, static_cast<std::size_t>(length)};
}

std::filesystem::path copy_path(const char* value, uint64_t length) {
  std::u8string utf8(static_cast<std::size_t>(length), u8'\0');
  if (length != 0)
    std::memcpy(utf8.data(), value, static_cast<std::size_t>(length));
  return std::filesystem::path{utf8};
}

std::vector<std::byte> read_binary_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return {};
  const auto size = stream.tellg();
  if (size <= 0)
    return {};
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), size);
  return stream ? bytes : std::vector<std::byte>{};
}

std::string read_text_file(const std::filesystem::path& path) {
  const auto bytes = read_binary_file(path);
  if (bytes.empty())
    return {};
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

template <typename Desc> bool valid_binding_expectations(const Desc& desc) {
  if (desc.struct_size < offsetof(Desc, validate_binding_set) + sizeof(desc.validate_binding_set))
    return true;
  if (desc.validate_binding_set > 1)
    return false;
  if (desc.validate_binding_set == 0)
    return true;
  if (desc.struct_size < sizeof(Desc) ||
      (desc.expected_binding_count != 0 && desc.expected_bindings == nullptr))
    return false;
  for (uint64_t index = 0; index < desc.expected_binding_count; ++index) {
    if (desc.expected_bindings[index].struct_size <
        sizeof(granit_asset_tools_shader_expected_binding))
      return false;
  }
  return true;
}

template <typename Desc>
bool validate_binding_expectations(const Desc& desc, const granit::tools::shader_info& info,
                                   std::ostream& diagnostic) {
  if (desc.struct_size < offsetof(Desc, validate_binding_set) + sizeof(desc.validate_binding_set) ||
      desc.validate_binding_set == 0)
    return true;
  std::vector<std::pair<uint32_t, uint32_t>> expected;
  expected.reserve(static_cast<std::size_t>(desc.expected_binding_count));
  for (uint64_t index = 0; index < desc.expected_binding_count; ++index)
    expected.emplace_back(desc.expected_bindings[index].group,
                          desc.expected_bindings[index].binding);
  std::ranges::sort(expected);
  if (std::ranges::adjacent_find(expected) != expected.end()) {
    diagnostic << "预期 Binding 集合包含重复项\n";
    return false;
  }
  std::vector<std::pair<uint32_t, uint32_t>> actual;
  actual.reserve(info.bindings.size());
  for (const auto& binding : info.bindings)
    actual.emplace_back(binding.group, binding.binding);
  if (expected == actual)
    return true;
  diagnostic << "WGSL 预期 Binding 集合与 SPIR-V 不一致：expected=" << expected.size()
             << " actual=" << actual.size() << '\n';
  return false;
}

uint32_t stage_value(const std::string& stage) {
  if (stage == "vertex")
    return GRANIT_SHADER_STAGE_VERTEX;
  if (stage == "fragment")
    return GRANIT_SHADER_STAGE_FRAGMENT;
  if (stage == "compute")
    return GRANIT_SHADER_STAGE_COMPUTE;
  return 0;
}

const char* stage_name(uint32_t stage) {
  switch (stage) {
  case GRANIT_SHADER_STAGE_VERTEX:
    return "vertex";
  case GRANIT_SHADER_STAGE_FRAGMENT:
    return "fragment";
  case GRANIT_SHADER_STAGE_COMPUTE:
    return "compute";
  default:
    return nullptr;
  }
}

uint32_t binding_type_value(granit::tools::shader_binding_type type) {
  using enum granit::tools::shader_binding_type;
  switch (type) {
  case uniform_buffer:
    return GRANIT_ASSET_TOOLS_SHADER_BINDING_UNIFORM_BUFFER;
  case storage_buffer:
    return GRANIT_ASSET_TOOLS_SHADER_BINDING_STORAGE_BUFFER;
  case sampled_texture:
    return GRANIT_ASSET_TOOLS_SHADER_BINDING_SAMPLED_TEXTURE;
  case storage_texture:
    return GRANIT_ASSET_TOOLS_SHADER_BINDING_STORAGE_TEXTURE;
  case sampler:
    return GRANIT_ASSET_TOOLS_SHADER_BINDING_SAMPLER;
  }
  return 0;
}

uint32_t binding_access_value(granit::tools::shader_binding_access access) {
  using enum granit::tools::shader_binding_access;
  switch (access) {
  case read:
    return GRANIT_ASSET_TOOLS_SHADER_ACCESS_READ;
  case write:
    return GRANIT_ASSET_TOOLS_SHADER_ACCESS_WRITE;
  case read_write:
    return GRANIT_ASSET_TOOLS_SHADER_ACCESS_READ_WRITE;
  }
  return 0;
}

uint32_t scalar_type_value(granit::tools::shader_scalar_type type) {
  using enum granit::tools::shader_scalar_type;
  switch (type) {
  case floating_point:
    return GRANIT_ASSET_TOOLS_SHADER_SCALAR_FLOAT;
  case signed_integer:
    return GRANIT_ASSET_TOOLS_SHADER_SCALAR_SINT;
  case unsigned_integer:
    return GRANIT_ASSET_TOOLS_SHADER_SCALAR_UINT;
  }
  return 0;
}

void store_reflection(stored_shader_data& target, granit::tools::shader_info& source) {
  target.reflection_json = granit::tools::serialize_shader_info_json(source);
  target.bindings = std::move(source.bindings);
  target.vertex_inputs = std::move(source.vertex_inputs);
  target.fragment_outputs = std::move(source.fragment_outputs);
  target.overrides = std::move(source.overrides);
  target.workgroup_size_x = source.workgroup_size_x;
  target.workgroup_size_y = source.workgroup_size_y;
  target.workgroup_size_z = source.workgroup_size_z;
}

std::shared_ptr<const stored_shader_data>
find_compilation(granit_asset_tools_shader_compilation compilation) {
  std::lock_guard lock{shader_data_mutex};
  const auto iterator = compilations.find(compilation);
  return iterator == compilations.end() ? nullptr : iterator->second;
}

std::shared_ptr<const stored_shader_data>
find_reflection(granit_asset_tools_shader_reflection reflection) {
  std::lock_guard lock{shader_data_mutex};
  const auto iterator = reflections.find(reflection);
  return iterator == reflections.end() ? nullptr : iterator->second;
}

uint64_t allocate_shader_data_handle() {
  auto handle = next_shader_data_handle.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0)
    handle = next_shader_data_handle.fetch_add(1, std::memory_order_relaxed);
  return handle;
}

granit_asset_tools_shader_compilation
store_compilation(std::shared_ptr<const stored_shader_data> value) {
  const auto handle = allocate_shader_data_handle();
  std::lock_guard lock{shader_data_mutex};
  compilations.emplace(handle, std::move(value));
  return handle;
}

granit_asset_tools_shader_reflection
store_reflection_handle(std::shared_ptr<const stored_shader_data> value) {
  const auto handle = allocate_shader_data_handle();
  std::lock_guard lock{shader_data_mutex};
  reflections.emplace(handle, std::move(value));
  return handle;
}

std::shared_ptr<const stored_compiler> find_compiler(granit_asset_tools_shader_compiler compiler) {
  std::lock_guard lock{compilers_mutex};
  const auto iterator = compilers.find(compiler);
  return iterator == compilers.end() ? nullptr : iterator->second;
}

} // namespace

extern "C" {

granit_result
granit_asset_tools_shader_compiler_create(const granit_asset_tools_shader_compiler_desc* desc,
                                          granit_asset_tools_shader_compiler* compiler) {
  if (compiler == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *compiler = 0;
  if (desc == nullptr || desc->struct_size < sizeof(*desc) || desc->reserved != 0 ||
      !valid_string(desc->toolchain_root, desc->toolchain_root_length) ||
      desc->toolchain_root_length == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    const auto paths = granit::asset_tools::detail::resolve_shader_toolchain(
        copy_path(desc->toolchain_root, desc->toolchain_root_length));
    if (!granit::asset_tools::detail::shader_toolchain_ready(paths))
      return GRANIT_ERROR_NOT_READY;
    auto value = std::make_shared<stored_compiler>();
    value->dxc = paths.dxc;
    value->tint = paths.tint;
    auto handle = next_compiler.fetch_add(1, std::memory_order_relaxed);
    if (handle == 0)
      handle = next_compiler.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock{compilers_mutex};
    compilers.emplace(handle, std::move(value));
    *compiler = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
granit_asset_tools_shader_compiler_compile(granit_asset_tools_shader_compiler compiler,
                                           const granit_asset_tools_shader_compile_desc* desc,
                                           granit_asset_tools_shader_compilation* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  const auto compiler_value = find_compiler(compiler);
  if (compiler_value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr || desc->struct_size < sizeof(*desc) || stage_name(desc->stage) == nullptr ||
      desc->target_backends == 0 ||
      (desc->target_backends & ~GRANIT_SHADER_BACKEND_ALL_BITS) != 0 ||
      !valid_string(desc->input_path, desc->input_path_length) ||
      !valid_string(desc->entry_point, desc->entry_point_length) ||
      !valid_string(desc->spirv_output_path, desc->spirv_output_path_length) ||
      !valid_string(desc->wgsl_output_path, desc->wgsl_output_path_length) ||
      desc->input_path_length == 0 || desc->entry_point_length == 0 ||
      desc->spirv_output_path_length == 0 || desc->wgsl_output_path_length == 0 ||
      !valid_binding_expectations(*desc))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (compiler_value->tint.empty() || compiler_value->dxc.empty())
    return GRANIT_ERROR_NOT_READY;
  try {
    std::vector<std::pair<std::string, std::string>> definitions;
    if (desc->define_count > 1024 || (desc->define_count != 0 && desc->defines == nullptr))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    definitions.reserve(desc->define_count);
    for (uint32_t index = 0; index < desc->define_count; ++index) {
      const auto& define = desc->defines[index];
      if (define.struct_size < sizeof(define) || define.reserved != 0 ||
          !valid_string(define.name, define.name_length) ||
          !valid_string(define.value, define.value_length))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      auto name = copy_string(define.name, define.name_length);
      auto value = copy_string(define.value, define.value_length);
      if (!valid_define_name(name) || value.empty() || value.find('\0') != std::string::npos)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      definitions.emplace_back(std::move(name), std::move(value));
    }
    std::ranges::sort(definitions);
    if (std::ranges::adjacent_find(definitions, [](const auto& left, const auto& right) {
          return left.first == right.first;
        }) != definitions.end())
      return GRANIT_ERROR_INVALID_ARGUMENT;

    auto value = std::make_shared<stored_shader_data>();
    std::ostringstream output;
    std::ostringstream diagnostic;
    granit::tools::shader_info info;
    const auto input = copy_path(desc->input_path, desc->input_path_length);
    const auto entry_point = copy_string(desc->entry_point, desc->entry_point_length);
    const auto stage = stage_name(desc->stage);
    const auto spirv_output = copy_path(desc->spirv_output_path, desc->spirv_output_path_length);
    const auto wgsl_output = copy_path(desc->wgsl_output_path, desc->wgsl_output_path_length);
    granit::tools::hlsl_compile_options options{
        compiler_value->dxc, compiler_value->tint,  input, entry_point, stage, spirv_output,
        wgsl_output,         std::move(definitions)};
    auto exit_code = granit::tools::compile_hlsl_shader(options, info, output, diagnostic);
    if (exit_code == 0 && !validate_binding_expectations(*desc, info, diagnostic)) {
      std::error_code filesystem_error;
      std::filesystem::remove(spirv_output, filesystem_error);
      if (!wgsl_output.empty())
        std::filesystem::remove(wgsl_output, filesystem_error);
      exit_code = 1;
    }
    value->status = exit_code == 0 ? GRANIT_SUCCESS : GRANIT_ERROR_INITIALIZATION_FAILED;
    value->entry_point = entry_point;
    value->stage = desc->stage;
    store_reflection(*value, info);
    value->output = std::move(output).str();
    value->diagnostic = std::move(diagnostic).str();
    if (exit_code == 0) {
      value->spirv = read_binary_file(spirv_output);
      value->wgsl = read_text_file(wgsl_output);
      if (value->spirv.empty() || value->wgsl.empty()) {
        value->status = GRANIT_ERROR_INITIALIZATION_FAILED;
        value->diagnostic += "无法读取编译产物\n";
      }
    }
    const auto status = value->status;
    *result = store_compilation(std::move(value));
    return status;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
granit_asset_tools_shader_compiler_destroy(granit_asset_tools_shader_compiler compiler) {
  std::lock_guard lock{compilers_mutex};
  return compilers.erase(compiler) == 1 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result
granit_asset_tools_shader_inspect_spirv(const granit_asset_tools_shader_inspect_desc* desc,
                                        granit_asset_tools_shader_reflection* reflection) {
  if (reflection == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *reflection = 0;
  if (desc == nullptr ||
      desc->struct_size < offsetof(granit_asset_tools_shader_inspect_desc, validate_binding_set) ||
      !valid_string(desc->input_path, desc->input_path_length) ||
      !valid_binding_expectations(*desc))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_shader_data>();
    std::ostringstream output;
    std::ostringstream diagnostic;
    granit::tools::shader_info info;
    const auto path = copy_path(desc->input_path, desc->input_path_length);
    const auto succeeded = granit::tools::inspect_shader(path, true, info, output, diagnostic) &&
                           validate_binding_expectations(*desc, info, diagnostic);
    value->status = succeeded ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
    value->entry_point = info.entry_point;
    value->stage = stage_value(info.stage);
    store_reflection(*value, info);
    value->output = std::move(output).str();
    value->diagnostic = std::move(diagnostic).str();
    const auto status = value->status;
    *reflection = store_reflection_handle(std::move(value));
    return status;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
granit_asset_tools_shader_compilation_get_info(granit_asset_tools_shader_compilation compilation,
                                               granit_asset_tools_shader_compilation_info* info) {
  if (info == nullptr || info->struct_size < sizeof(*info))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_compilation(compilation);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  info->status = value->status;
  info->entry_point = value->entry_point.data();
  info->entry_point_length = value->entry_point.size();
  info->stage = value->stage;
  info->output = value->output.data();
  info->output_length = value->output.size();
  info->diagnostic = value->diagnostic.data();
  info->diagnostic_length = value->diagnostic.size();
  return GRANIT_SUCCESS;
}

granit_result granit_asset_tools_shader_compilation_get_reflection(
    granit_asset_tools_shader_compilation compilation,
    granit_asset_tools_shader_reflection* reflection) {
  if (reflection == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *reflection = 0;
  const auto value = find_compilation(compilation);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    *reflection = store_reflection_handle(value);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
granit_asset_tools_shader_compilation_get_spirv(granit_asset_tools_shader_compilation compilation,
                                                const void** data, uint64_t* size) {
  if (data == nullptr || size == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *data = nullptr;
  *size = 0;
  const auto value = find_compilation(compilation);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *data = value->spirv.data();
  *size = value->spirv.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_shader_compilation_get_wgsl(granit_asset_tools_shader_compilation compilation,
                                               const char** source, uint64_t* length) {
  if (source == nullptr || length == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *source = nullptr;
  *length = 0;
  const auto value = find_compilation(compilation);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *source = value->wgsl.data();
  *length = value->wgsl.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_shader_reflection_get_info(granit_asset_tools_shader_reflection reflection,
                                              granit_asset_tools_shader_reflection_info* info) {
  if (info == nullptr || info->struct_size < sizeof(*info))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  info->status = value->status;
  info->entry_point = value->entry_point.data();
  info->entry_point_length = value->entry_point.size();
  info->stage = value->stage;
  info->output = value->output.data();
  info->output_length = value->output.size();
  info->diagnostic = value->diagnostic.data();
  info->diagnostic_length = value->diagnostic.size();
  return GRANIT_SUCCESS;
}

granit_result granit_asset_tools_shader_reflection_get_binding_count(
    granit_asset_tools_shader_reflection reflection, uint64_t* count) {
  if (count == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *count = value->bindings.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_shader_reflection_get_binding(granit_asset_tools_shader_reflection reflection,
                                                 uint64_t index,
                                                 granit_asset_tools_shader_binding_info* binding) {
  if (binding == nullptr || binding->struct_size < sizeof(*binding))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (index >= value->bindings.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& source = value->bindings[static_cast<std::size_t>(index)];
  binding->group = source.group;
  binding->binding = source.binding;
  binding->type = binding_type_value(source.type);
  binding->access = binding_access_value(source.access);
  binding->name = source.name.data();
  binding->name_length = source.name.size();
  binding->array_count = source.array_count;
  binding->minimum_binding_size = source.minimum_binding_size;
  return GRANIT_SUCCESS;
}

granit_result get_interface_variable_count(
    granit_asset_tools_shader_reflection reflection,
    const std::vector<granit::tools::shader_interface_variable_info> stored_shader_data::* member,
    uint64_t* count) {
  if (count == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *count = (value.get()->*member).size();
  return GRANIT_SUCCESS;
}

granit_result get_interface_variable(
    granit_asset_tools_shader_reflection reflection, uint64_t index,
    const std::vector<granit::tools::shader_interface_variable_info> stored_shader_data::* member,
    granit_asset_tools_shader_interface_variable_info* output) {
  if (output == nullptr || output->struct_size < sizeof(*output))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& variables = value.get()->*member;
  if (index >= variables.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& source = variables[static_cast<std::size_t>(index)];
  output->location = source.location;
  output->component = source.component;
  output->scalar_type = scalar_type_value(source.scalar_type);
  output->bit_width = source.bit_width;
  output->vector_size = source.vector_size;
  output->name = source.name.data();
  output->name_length = source.name.size();
  return GRANIT_SUCCESS;
}

granit_result granit_asset_tools_shader_reflection_get_vertex_input_count(
    granit_asset_tools_shader_reflection reflection, uint64_t* count) {
  return get_interface_variable_count(reflection, &stored_shader_data::vertex_inputs, count);
}

granit_result granit_asset_tools_shader_reflection_get_vertex_input(
    granit_asset_tools_shader_reflection reflection, uint64_t index,
    granit_asset_tools_shader_interface_variable_info* input) {
  return get_interface_variable(reflection, index, &stored_shader_data::vertex_inputs, input);
}

granit_result granit_asset_tools_shader_reflection_get_fragment_output_count(
    granit_asset_tools_shader_reflection reflection, uint64_t* count) {
  return get_interface_variable_count(reflection, &stored_shader_data::fragment_outputs, count);
}

granit_result granit_asset_tools_shader_reflection_get_fragment_output(
    granit_asset_tools_shader_reflection reflection, uint64_t index,
    granit_asset_tools_shader_interface_variable_info* output) {
  return get_interface_variable(reflection, index, &stored_shader_data::fragment_outputs, output);
}

granit_result granit_asset_tools_shader_reflection_get_workgroup_size(
    granit_asset_tools_shader_reflection reflection,
    granit_asset_tools_shader_workgroup_size* size) {
  if (size == nullptr || size->struct_size < sizeof(*size))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  size->x = value->workgroup_size_x;
  size->y = value->workgroup_size_y;
  size->z = value->workgroup_size_z;
  return GRANIT_SUCCESS;
}

granit_result granit_asset_tools_shader_reflection_get_override_count(
    granit_asset_tools_shader_reflection reflection, uint64_t* count) {
  if (count == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *count = value->overrides.size();
  return GRANIT_SUCCESS;
}

granit_result granit_asset_tools_shader_reflection_get_override(
    granit_asset_tools_shader_reflection reflection, uint64_t index,
    granit_asset_tools_shader_override_info* override_info) {
  if (override_info == nullptr || override_info->struct_size < sizeof(*override_info))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (index >= value->overrides.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& source = value->overrides[static_cast<std::size_t>(index)];
  override_info->id = source.id;
  override_info->scalar_type = scalar_type_value(source.scalar_type);
  override_info->bit_width = source.bit_width;
  override_info->name = source.name.data();
  override_info->name_length = source.name.size();
  override_info->default_value = source.default_value;
  override_info->default_value_size = source.default_value_size;
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_shader_reflection_get_json(granit_asset_tools_shader_reflection reflection,
                                              const char** json, uint64_t* length) {
  if (json == nullptr || length == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *json = nullptr;
  *length = 0;
  const auto value = find_reflection(reflection);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *json = value->reflection_json.data();
  *length = value->reflection_json.size();
  return GRANIT_SUCCESS;
}

granit_result granit_asset_tools_shader_get_target_capabilities(
    granit_shader_backend_flags backend, granit_shader_profile profile,
    granit_asset_tools_shader_target_capabilities* capabilities) {
  if (capabilities == nullptr ||
      capabilities->struct_size < sizeof(granit_asset_tools_shader_target_capabilities) ||
      capabilities->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if ((backend != GRANIT_SHADER_BACKEND_VULKAN_BIT &&
       backend != GRANIT_SHADER_BACKEND_WEBGPU_BIT) ||
      profile != GRANIT_SHADER_PROFILE_PORTABLE)
    return GRANIT_ERROR_UNSUPPORTED;
  capabilities->backend = backend;
  capabilities->profile = profile;
  capabilities->reserved = 0;
  capabilities->supported_features = 0;
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_shader_compilation_destroy(granit_asset_tools_shader_compilation compilation) {
  std::lock_guard lock{shader_data_mutex};
  return compilations.erase(compilation) == 1 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result
granit_asset_tools_shader_reflection_destroy(granit_asset_tools_shader_reflection reflection) {
  std::lock_guard lock{shader_data_mutex};
  return reflections.erase(reflection) == 1 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

} // extern "C"
