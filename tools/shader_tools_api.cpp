// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.h>

#include "shader_tools_core.h"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {

struct stored_result {
  granit_result status = GRANIT_ERROR_INTERNAL;
  std::string entry_point;
  uint32_t stage = 0;
  std::string output;
  std::string diagnostic;
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
  if (desc == nullptr || desc->struct_size < sizeof(*desc) ||
      !valid_string(desc->tint_path, desc->tint_path_length) ||
      !valid_string(desc->input_path, desc->input_path_length) ||
      !valid_string(desc->entry_point, desc->entry_point_length) ||
      !valid_string(desc->output_path, desc->output_path_length) ||
      stage_name(desc->stage) == nullptr)
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
    const auto exit_code = granit::tools::compile_shader(options, output, diagnostic);
    value->status = exit_code == 0 ? GRANIT_SUCCESS : GRANIT_ERROR_INITIALIZATION_FAILED;
    value->entry_point = options.entry_point;
    value->stage = desc->stage;
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
  if (desc == nullptr || desc->struct_size < sizeof(*desc) ||
      !valid_string(desc->input_path, desc->input_path_length))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_result>();
    std::ostringstream output;
    std::ostringstream diagnostic;
    granit::tools::shader_info info;
    const auto path = copy_path(desc->input_path, desc->input_path_length);
    const auto succeeded = granit::tools::inspect_shader(path, true, info, output, diagnostic);
    value->status = succeeded ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
    value->entry_point = std::move(info.entry_point);
    value->stage = stage_value(info.stage);
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
  std::shared_ptr<const stored_result> value;
  {
    std::lock_guard lock{results_mutex};
    const auto iterator = results.find(result);
    if (iterator == results.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    value = iterator->second;
  }
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

granit_result granit_shader_tools_result_destroy(granit_shader_tools_result result) {
  std::lock_guard lock{results_mutex};
  return results.erase(result) == 1 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

} // extern "C"
