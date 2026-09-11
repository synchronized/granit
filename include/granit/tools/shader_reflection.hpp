// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_REFLECTION_HPP_
#define GRANIT_SHADER_REFLECTION_HPP_

#include <granit/core/result.hpp>
#include <granit/core/shader_types.hpp>
#include <granit/tools/shader_reflection.h>

#include <cstddef>
#include <string_view>
#include <utility>

namespace granit::shader_tools {

struct reflection_info {
  ::granit::result status = ::granit::result::invalid_handle;
  std::string_view entry_point;
  shader_stage stage = shader_stage::vertex;
  std::string_view output;
  std::string_view diagnostic;
};

enum class binding_type : std::uint32_t {
  uniform_buffer = GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER,
  storage_buffer = GRANIT_SHADER_TOOLS_BINDING_STORAGE_BUFFER,
  sampled_texture = GRANIT_SHADER_TOOLS_BINDING_SAMPLED_TEXTURE,
  storage_texture = GRANIT_SHADER_TOOLS_BINDING_STORAGE_TEXTURE,
  sampler = GRANIT_SHADER_TOOLS_BINDING_SAMPLER,
};

enum class binding_access : std::uint32_t {
  read = GRANIT_SHADER_TOOLS_ACCESS_READ,
  write = GRANIT_SHADER_TOOLS_ACCESS_WRITE,
  read_write = GRANIT_SHADER_TOOLS_ACCESS_READ_WRITE,
};

enum class scalar_type : std::uint32_t {
  floating_point = GRANIT_SHADER_TOOLS_SCALAR_FLOAT,
  signed_integer = GRANIT_SHADER_TOOLS_SCALAR_SINT,
  unsigned_integer = GRANIT_SHADER_TOOLS_SCALAR_UINT,
};

struct binding_info {
  uint32_t group = 0;
  uint32_t binding = 0;
  binding_type type = binding_type::uniform_buffer;
  binding_access access = binding_access::read;
  std::string_view name;
  uint32_t array_count = 0;
  uint64_t minimum_binding_size = 0;
};

struct interface_variable_info {
  uint32_t location = 0;
  uint32_t component = 0;
  enum scalar_type scalar_type = scalar_type::floating_point;
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
  enum scalar_type scalar_type = scalar_type::floating_point;
  uint32_t bit_width = 0;
  std::string_view name;
  uint64_t default_value = 0;
  uint32_t default_value_size = 0;
};

class reflection {
public:
  reflection() = default;
  explicit reflection(granit_shader_tools_reflection handle) noexcept : handle_(handle) {}
  ~reflection() { reset(); }
  reflection(const reflection&) = delete;
  reflection& operator=(const reflection&) = delete;
  reflection(reflection&& other) noexcept : handle_(std::exchange(other.handle_, 0)) {}
  reflection& operator=(reflection&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != 0; }
  [[nodiscard]] reflection_info info() const noexcept {
    granit_shader_tools_reflection_info value{};
    value.struct_size = sizeof(value);
    if (granit_shader_tools_reflection_get_info(handle_, &value) != GRANIT_SUCCESS)
      return {};
    return {::granit::from_native(value.status),
            {value.entry_point, static_cast<std::size_t>(value.entry_point_length)},
            static_cast<shader_stage>(value.stage),
            {value.output, static_cast<std::size_t>(value.output_length)},
            {value.diagnostic, static_cast<std::size_t>(value.diagnostic_length)}};
  }
  [[nodiscard]] uint64_t binding_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_reflection_get_binding_count(handle_, &count) == GRANIT_SUCCESS
               ? count
               : 0;
  }
  [[nodiscard]] std::pair<::granit::result, binding_info> binding(uint64_t index) const noexcept {
    granit_shader_tools_binding_info value{};
    value.struct_size = sizeof(value);
    const auto status = granit_shader_tools_reflection_get_binding(handle_, index, &value);
    if (status != GRANIT_SUCCESS)
      return {::granit::from_native(status), {}};
    return {::granit::result::success,
            {value.group,
             value.binding,
             static_cast<binding_type>(value.type),
             static_cast<binding_access>(value.access),
             {value.name, static_cast<std::size_t>(value.name_length)},
             value.array_count,
             value.minimum_binding_size}};
  }
  [[nodiscard]] uint64_t vertex_input_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_reflection_get_vertex_input_count(handle_, &count) == GRANIT_SUCCESS
               ? count
               : 0;
  }
  [[nodiscard]] uint64_t fragment_output_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_reflection_get_fragment_output_count(handle_, &count) ==
                   GRANIT_SUCCESS
               ? count
               : 0;
  }
  [[nodiscard]] std::pair<::granit::result, interface_variable_info>
  vertex_input(uint64_t index) const noexcept {
    return interface_variable(index, granit_shader_tools_reflection_get_vertex_input);
  }
  [[nodiscard]] std::pair<::granit::result, interface_variable_info>
  fragment_output(uint64_t index) const noexcept {
    return interface_variable(index, granit_shader_tools_reflection_get_fragment_output);
  }
  [[nodiscard]] workgroup_size compute_workgroup_size() const noexcept {
    granit_shader_tools_workgroup_size value{};
    value.struct_size = sizeof(value);
    if (granit_shader_tools_reflection_get_workgroup_size(handle_, &value) != GRANIT_SUCCESS)
      return {};
    return {value.x, value.y, value.z};
  }
  [[nodiscard]] uint64_t override_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_reflection_get_override_count(handle_, &count) == GRANIT_SUCCESS
               ? count
               : 0;
  }
  [[nodiscard]] std::pair<::granit::result, override_info>
  override_at(uint64_t index) const noexcept {
    granit_shader_tools_override_info value{};
    value.struct_size = sizeof(value);
    const auto status = granit_shader_tools_reflection_get_override(handle_, index, &value);
    if (status != GRANIT_SUCCESS)
      return {::granit::from_native(status), {}};
    return {::granit::result::success,
            {value.id,
             static_cast<scalar_type>(value.scalar_type),
             value.bit_width,
             {value.name, static_cast<std::size_t>(value.name_length)},
             value.default_value,
             value.default_value_size}};
  }
  [[nodiscard]] std::string_view reflection_json() const noexcept {
    const char* data = nullptr;
    uint64_t size = 0;
    if (granit_shader_tools_reflection_get_json(handle_, &data, &size) != GRANIT_SUCCESS)
      return {};
    return {data, static_cast<std::size_t>(size)};
  }
  [[nodiscard]] std::pair<::granit::result, bool>
  write_asset(const granit_shader_tools_asset_desc& desc) const noexcept {
    uint32_t cache_hit = 0;
    const auto status = granit_shader_tools_reflection_write_asset(handle_, &desc, &cache_hit);
    return {::granit::from_native(status), cache_hit != 0};
  }
  void reset() noexcept {
    if (handle_ != 0) {
      granit_shader_tools_reflection_destroy(handle_);
      handle_ = 0;
    }
  }

private:
  using interface_getter = granit_result (*)(granit_shader_tools_reflection, uint64_t,
                                             granit_shader_tools_interface_variable_info*);

  [[nodiscard]] std::pair<::granit::result, interface_variable_info>
  interface_variable(uint64_t index, interface_getter getter) const noexcept {
    granit_shader_tools_interface_variable_info value{};
    value.struct_size = sizeof(value);
    const auto status = getter(handle_, index, &value);
    if (status != GRANIT_SUCCESS)
      return {::granit::from_native(status), {}};
    return {::granit::result::success,
            {value.location,
             value.component,
             static_cast<scalar_type>(value.scalar_type),
             value.bit_width,
             value.vector_size,
             {value.name, static_cast<std::size_t>(value.name_length)}}};
  }

  granit_shader_tools_reflection handle_ = 0;
};

inline std::pair<::granit::result, reflection>
inspect_spirv(const granit_shader_tools_inspect_desc& desc) noexcept {
  granit_shader_tools_reflection handle = 0;
  const auto status = granit_shader_tools_inspect_spirv(&desc, &handle);
  return {::granit::from_native(status), reflection{handle}};
}

} // namespace granit::shader_tools

#endif
