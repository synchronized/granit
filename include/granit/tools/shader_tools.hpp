// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_TOOLS_HPP_
#define GRANIT_SHADER_TOOLS_HPP_

#include <granit/tools/shader_tools.h>

#include <string_view>
#include <utility>

namespace granit::shader_tools {

struct result_info {
  granit_result status = GRANIT_ERROR_INVALID_HANDLE;
  std::string_view entry_point;
  uint32_t stage = 0;
  std::string_view output;
  std::string_view diagnostic;
};

struct binding_info {
  uint32_t group = 0;
  uint32_t binding = 0;
  uint32_t type = 0;
  uint32_t access = 0;
  std::string_view name;
  uint32_t array_count = 0;
  uint64_t minimum_binding_size = 0;
};

struct interface_variable_info {
  uint32_t location = 0;
  uint32_t component = 0;
  uint32_t scalar_type = 0;
  uint32_t bit_width = 0;
  uint32_t vector_size = 0;
  std::string_view name;
};

struct workgroup_size {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;
};

struct override_info {
  uint32_t id = 0;
  uint32_t scalar_type = 0;
  uint32_t bit_width = 0;
  std::string_view name;
  uint64_t default_value = 0;
  uint32_t default_value_size = 0;
};

class result {
public:
  result() = default;
  explicit result(granit_shader_tools_result handle) noexcept : handle_(handle) {}
  ~result() { reset(); }
  result(const result&) = delete;
  result& operator=(const result&) = delete;
  result(result&& other) noexcept : handle_(std::exchange(other.handle_, 0)) {}
  result& operator=(result&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != 0; }
  [[nodiscard]] result_info info() const noexcept {
    granit_shader_tools_result_info value{};
    value.struct_size = sizeof(value);
    if (granit_shader_tools_result_get_info(handle_, &value) != GRANIT_SUCCESS)
      return {};
    return {value.status,
            {value.entry_point, static_cast<std::size_t>(value.entry_point_length)},
            value.stage,
            {value.output, static_cast<std::size_t>(value.output_length)},
            {value.diagnostic, static_cast<std::size_t>(value.diagnostic_length)}};
  }
  [[nodiscard]] uint64_t binding_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_result_get_binding_count(handle_, &count) == GRANIT_SUCCESS ? count
                                                                                           : 0;
  }
  [[nodiscard]] std::pair<granit_result, binding_info> binding(uint64_t index) const noexcept {
    granit_shader_tools_binding_info value{};
    value.struct_size = sizeof(value);
    const auto status = granit_shader_tools_result_get_binding(handle_, index, &value);
    if (status != GRANIT_SUCCESS)
      return {status, {}};
    return {GRANIT_SUCCESS,
            {value.group,
             value.binding,
             value.type,
             value.access,
             {value.name, static_cast<std::size_t>(value.name_length)},
             value.array_count,
             value.minimum_binding_size}};
  }
  [[nodiscard]] uint64_t vertex_input_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_result_get_vertex_input_count(handle_, &count) == GRANIT_SUCCESS
               ? count
               : 0;
  }
  [[nodiscard]] uint64_t fragment_output_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_result_get_fragment_output_count(handle_, &count) == GRANIT_SUCCESS
               ? count
               : 0;
  }
  [[nodiscard]] std::pair<granit_result, interface_variable_info>
  vertex_input(uint64_t index) const noexcept {
    return interface_variable(index, granit_shader_tools_result_get_vertex_input);
  }
  [[nodiscard]] std::pair<granit_result, interface_variable_info>
  fragment_output(uint64_t index) const noexcept {
    return interface_variable(index, granit_shader_tools_result_get_fragment_output);
  }
  [[nodiscard]] workgroup_size compute_workgroup_size() const noexcept {
    granit_shader_tools_workgroup_size value{};
    value.struct_size = sizeof(value);
    if (granit_shader_tools_result_get_workgroup_size(handle_, &value) != GRANIT_SUCCESS)
      return {};
    return {value.x, value.y, value.z};
  }
  [[nodiscard]] uint64_t override_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_result_get_override_count(handle_, &count) == GRANIT_SUCCESS ? count
                                                                                            : 0;
  }
  [[nodiscard]] std::pair<granit_result, override_info> override_at(uint64_t index) const noexcept {
    granit_shader_tools_override_info value{};
    value.struct_size = sizeof(value);
    const auto status = granit_shader_tools_result_get_override(handle_, index, &value);
    if (status != GRANIT_SUCCESS)
      return {status, {}};
    return {GRANIT_SUCCESS,
            {value.id,
             value.scalar_type,
             value.bit_width,
             {value.name, static_cast<std::size_t>(value.name_length)},
             value.default_value,
             value.default_value_size}};
  }
  [[nodiscard]] std::string_view reflection_json() const noexcept {
    const char* data = nullptr;
    uint64_t size = 0;
    if (granit_shader_tools_result_get_reflection_json(handle_, &data, &size) != GRANIT_SUCCESS)
      return {};
    return {data, static_cast<std::size_t>(size)};
  }
  void reset() noexcept {
    if (handle_ != 0) {
      granit_shader_tools_result_destroy(handle_);
      handle_ = 0;
    }
  }

private:
  using interface_getter = granit_result (*)(granit_shader_tools_result, uint64_t,
                                             granit_shader_tools_interface_variable_info*);

  [[nodiscard]] std::pair<granit_result, interface_variable_info>
  interface_variable(uint64_t index, interface_getter getter) const noexcept {
    granit_shader_tools_interface_variable_info value{};
    value.struct_size = sizeof(value);
    const auto status = getter(handle_, index, &value);
    if (status != GRANIT_SUCCESS)
      return {status, {}};
    return {GRANIT_SUCCESS,
            {value.location,
             value.component,
             value.scalar_type,
             value.bit_width,
             value.vector_size,
             {value.name, static_cast<std::size_t>(value.name_length)}}};
  }

  granit_shader_tools_result handle_ = 0;
};

inline std::pair<granit_result, result>
compile_wgsl(const granit_shader_tools_compile_desc& desc) noexcept {
  granit_shader_tools_result handle = 0;
  const auto status = granit_shader_tools_compile_wgsl(&desc, &handle);
  return {status, result{handle}};
}

inline std::pair<granit_result, result>
inspect_spirv(const granit_shader_tools_inspect_desc& desc) noexcept {
  granit_shader_tools_result handle = 0;
  const auto status = granit_shader_tools_inspect_spirv(&desc, &handle);
  return {status, result{handle}};
}

} // namespace granit::shader_tools

#endif
