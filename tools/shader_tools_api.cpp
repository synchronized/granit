// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.h>

#include "shader_tools_core.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct stored_result {
  granit_result status = GRANIT_ERROR_INTERNAL;
  std::string entry_point;
  uint32_t stage = 0;
  std::string output;
  std::string diagnostic;
  std::vector<granit::tools::shader_binding_info> bindings;
  std::vector<granit::tools::shader_interface_variable_info> vertex_inputs;
  std::vector<granit::tools::shader_interface_variable_info> fragment_outputs;
  std::vector<granit::tools::shader_override_info> overrides;
  uint32_t workgroup_size_x = 0;
  uint32_t workgroup_size_y = 0;
  uint32_t workgroup_size_z = 0;
};

std::mutex results_mutex;
std::unordered_map<uint64_t, std::shared_ptr<const stored_result>> results;
std::atomic<uint64_t> next_result{1};

bool valid_string(const char* value, uint64_t length) { return value != nullptr || length == 0; }

std::string copy_string(const char* value, uint64_t length) {
  return length == 0 ? std::string{} : std::string{value, static_cast<std::size_t>(length)};
}

std::filesystem::path copy_path(const char* value, uint64_t length) {
  std::u8string utf8(static_cast<std::size_t>(length), u8'\0');
  if (length != 0)
    std::memcpy(utf8.data(), value, static_cast<std::size_t>(length));
  return std::filesystem::path{utf8};
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
    if (desc.expected_bindings[index].struct_size < sizeof(granit_shader_tools_expected_binding))
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
    return GRANIT_SHADER_TOOLS_STAGE_VERTEX;
  if (stage == "fragment")
    return GRANIT_SHADER_TOOLS_STAGE_FRAGMENT;
  if (stage == "compute")
    return GRANIT_SHADER_TOOLS_STAGE_COMPUTE;
  return 0;
}

const char* stage_name(uint32_t stage) {
  switch (stage) {
  case GRANIT_SHADER_TOOLS_STAGE_VERTEX:
    return "vertex";
  case GRANIT_SHADER_TOOLS_STAGE_FRAGMENT:
    return "fragment";
  case GRANIT_SHADER_TOOLS_STAGE_COMPUTE:
    return "compute";
  default:
    return nullptr;
  }
}

uint32_t binding_type_value(granit::tools::shader_binding_type type) {
  using enum granit::tools::shader_binding_type;
  switch (type) {
  case uniform_buffer:
    return GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER;
  case storage_buffer:
    return GRANIT_SHADER_TOOLS_BINDING_STORAGE_BUFFER;
  case sampled_texture:
    return GRANIT_SHADER_TOOLS_BINDING_SAMPLED_TEXTURE;
  case storage_texture:
    return GRANIT_SHADER_TOOLS_BINDING_STORAGE_TEXTURE;
  case sampler:
    return GRANIT_SHADER_TOOLS_BINDING_SAMPLER;
  }
  return 0;
}

uint32_t binding_access_value(granit::tools::shader_binding_access access) {
  using enum granit::tools::shader_binding_access;
  switch (access) {
  case read:
    return GRANIT_SHADER_TOOLS_ACCESS_READ;
  case write:
    return GRANIT_SHADER_TOOLS_ACCESS_WRITE;
  case read_write:
    return GRANIT_SHADER_TOOLS_ACCESS_READ_WRITE;
  }
  return 0;
}

uint32_t scalar_type_value(granit::tools::shader_scalar_type type) {
  using enum granit::tools::shader_scalar_type;
  switch (type) {
  case floating_point:
    return GRANIT_SHADER_TOOLS_SCALAR_FLOAT;
  case signed_integer:
    return GRANIT_SHADER_TOOLS_SCALAR_SINT;
  case unsigned_integer:
    return GRANIT_SHADER_TOOLS_SCALAR_UINT;
  }
  return 0;
}

void store_reflection(stored_result& target, granit::tools::shader_info& source) {
  target.bindings = std::move(source.bindings);
  target.vertex_inputs = std::move(source.vertex_inputs);
  target.fragment_outputs = std::move(source.fragment_outputs);
  target.overrides = std::move(source.overrides);
  target.workgroup_size_x = source.workgroup_size_x;
  target.workgroup_size_y = source.workgroup_size_y;
  target.workgroup_size_z = source.workgroup_size_z;
}

std::shared_ptr<const stored_result> find_result(granit_shader_tools_result result) {
  std::lock_guard lock{results_mutex};
  const auto iterator = results.find(result);
  return iterator == results.end() ? nullptr : iterator->second;
}

granit_shader_tools_result store(std::shared_ptr<const stored_result> value) {
  auto handle = next_result.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0)
    handle = next_result.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard lock{results_mutex};
  results.emplace(handle, std::move(value));
  return handle;
}

} // namespace

extern "C" {

granit_result granit_shader_tools_compile_wgsl(const granit_shader_tools_compile_desc* desc,
                                               granit_shader_tools_result* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  if (desc == nullptr ||
      desc->struct_size < offsetof(granit_shader_tools_compile_desc, validate_binding_set) ||
      !valid_string(desc->tint_path, desc->tint_path_length) ||
      !valid_string(desc->input_path, desc->input_path_length) ||
      !valid_string(desc->entry_point, desc->entry_point_length) ||
      !valid_string(desc->output_path, desc->output_path_length) ||
      stage_name(desc->stage) == nullptr || !valid_binding_expectations(*desc))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_result>();
    std::ostringstream output;
    std::ostringstream diagnostic;
    granit::tools::compile_options options{copy_path(desc->tint_path, desc->tint_path_length),
                                           copy_path(desc->input_path, desc->input_path_length),
                                           copy_string(desc->entry_point, desc->entry_point_length),
                                           stage_name(desc->stage),
                                           copy_path(desc->output_path, desc->output_path_length)};
    granit::tools::shader_info info;
    auto exit_code = granit::tools::compile_shader(options, info, output, diagnostic);
    if (exit_code == 0 && !validate_binding_expectations(*desc, info, diagnostic)) {
      std::error_code filesystem_error;
      std::filesystem::remove(options.output, filesystem_error);
      exit_code = 1;
    }
    value->status = exit_code == 0 ? GRANIT_SUCCESS : GRANIT_ERROR_INITIALIZATION_FAILED;
    value->entry_point = options.entry_point;
    value->stage = desc->stage;
    store_reflection(*value, info);
    value->output = std::move(output).str();
    value->diagnostic = std::move(diagnostic).str();
    const auto status = value->status;
    *result = store(std::move(value));
    return status;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result granit_shader_tools_inspect_spirv(const granit_shader_tools_inspect_desc* desc,
                                                granit_shader_tools_result* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  if (desc == nullptr ||
      desc->struct_size < offsetof(granit_shader_tools_inspect_desc, validate_binding_set) ||
      !valid_string(desc->input_path, desc->input_path_length) ||
      !valid_binding_expectations(*desc))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_result>();
    std::ostringstream output;
    std::ostringstream diagnostic;
    granit::tools::shader_info info;
    const auto path = copy_path(desc->input_path, desc->input_path_length);
    const auto succeeded = granit::tools::inspect_shader(path, true, info, output, diagnostic) &&
                           validate_binding_expectations(*desc, info, diagnostic);
    value->status = succeeded ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
    value->entry_point = std::move(info.entry_point);
    value->stage = stage_value(info.stage);
    store_reflection(*value, info);
    value->output = std::move(output).str();
    value->diagnostic = std::move(diagnostic).str();
    const auto status = value->status;
    *result = store(std::move(value));
    return status;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result granit_shader_tools_result_get_info(granit_shader_tools_result result,
                                                  granit_shader_tools_result_info* info) {
  if (info == nullptr || info->struct_size < sizeof(*info))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_result(result);
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

granit_result granit_shader_tools_result_get_binding_count(granit_shader_tools_result result,
                                                           uint64_t* count) {
  if (count == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_result(result);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *count = value->bindings.size();
  return GRANIT_SUCCESS;
}

granit_result granit_shader_tools_result_get_binding(granit_shader_tools_result result,
                                                     uint64_t index,
                                                     granit_shader_tools_binding_info* binding) {
  if (binding == nullptr || binding->struct_size < sizeof(*binding))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_result(result);
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
    granit_shader_tools_result result,
    const std::vector<granit::tools::shader_interface_variable_info> stored_result::* member,
    uint64_t* count) {
  if (count == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_result(result);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *count = (value.get()->*member).size();
  return GRANIT_SUCCESS;
}

granit_result get_interface_variable(
    granit_shader_tools_result result, uint64_t index,
    const std::vector<granit::tools::shader_interface_variable_info> stored_result::* member,
    granit_shader_tools_interface_variable_info* output) {
  if (output == nullptr || output->struct_size < sizeof(*output))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_result(result);
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

granit_result granit_shader_tools_result_get_vertex_input_count(granit_shader_tools_result result,
                                                                uint64_t* count) {
  return get_interface_variable_count(result, &stored_result::vertex_inputs, count);
}

granit_result
granit_shader_tools_result_get_vertex_input(granit_shader_tools_result result, uint64_t index,
                                            granit_shader_tools_interface_variable_info* input) {
  return get_interface_variable(result, index, &stored_result::vertex_inputs, input);
}

granit_result
granit_shader_tools_result_get_fragment_output_count(granit_shader_tools_result result,
                                                     uint64_t* count) {
  return get_interface_variable_count(result, &stored_result::fragment_outputs, count);
}

granit_result granit_shader_tools_result_get_fragment_output(
    granit_shader_tools_result result, uint64_t index,
    granit_shader_tools_interface_variable_info* output) {
  return get_interface_variable(result, index, &stored_result::fragment_outputs, output);
}

granit_result
granit_shader_tools_result_get_workgroup_size(granit_shader_tools_result result,
                                              granit_shader_tools_workgroup_size* size) {
  if (size == nullptr || size->struct_size < sizeof(*size))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_result(result);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  size->x = value->workgroup_size_x;
  size->y = value->workgroup_size_y;
  size->z = value->workgroup_size_z;
  return GRANIT_SUCCESS;
}

granit_result granit_shader_tools_result_get_override_count(granit_shader_tools_result result,
                                                            uint64_t* count) {
  if (count == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_result(result);
  if (!value)
    return GRANIT_ERROR_INVALID_HANDLE;
  *count = value->overrides.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_shader_tools_result_get_override(granit_shader_tools_result result, uint64_t index,
                                        granit_shader_tools_override_info* override_info) {
  if (override_info == nullptr || override_info->struct_size < sizeof(*override_info))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_result(result);
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

granit_result granit_shader_tools_result_destroy(granit_shader_tools_result result) {
  std::lock_guard lock{results_mutex};
  return results.erase(result) == 1 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

} // extern "C"
