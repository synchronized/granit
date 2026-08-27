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
  void reset() noexcept {
    if (handle_ != 0) {
      granit_shader_tools_result_destroy(handle_);
      handle_ = 0;
    }
  }

private:
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
