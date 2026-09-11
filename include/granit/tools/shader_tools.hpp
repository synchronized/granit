// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_TOOLS_HPP_
#define GRANIT_SHADER_TOOLS_HPP_

#include <granit/core/result.hpp>
#include <granit/core/shader_types.hpp>
#include <granit/tools/shader_tools.h>

#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::shader_tools {

struct result_info {
  ::granit::result status = ::granit::result::invalid_handle;
  std::string_view entry_point;
  shader_stage stage = shader_stage::vertex;
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

struct compiler_config {
  std::string_view dxc_path;
  std::string_view tint_path;
};

struct shader_define {
  std::string_view name;
  std::string_view value;
};

struct compile_desc {
  std::string_view input_path;
  shader_source_language source_language{shader_source_language::wgsl};
  shader_stage stage{shader_stage::vertex};
  std::string_view entry_point{"main"};
  shader_backend target_backends{shader_backend::all};
  std::string_view spirv_output_path;
  std::string_view wgsl_output_path;
  std::span<const shader_define> defines;
  bool validate_binding_set{};
  std::span<const granit_shader_tools_expected_binding> expected_bindings;
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
    return {::granit::from_native(value.status),
            {value.entry_point, static_cast<std::size_t>(value.entry_point_length)},
            static_cast<shader_stage>(value.stage),
            {value.output, static_cast<std::size_t>(value.output_length)},
            {value.diagnostic, static_cast<std::size_t>(value.diagnostic_length)}};
  }
  [[nodiscard]] uint64_t binding_count() const noexcept {
    uint64_t count = 0;
    return granit_shader_tools_result_get_binding_count(handle_, &count) == GRANIT_SUCCESS ? count
                                                                                           : 0;
  }
  [[nodiscard]] std::pair<::granit::result, binding_info> binding(uint64_t index) const noexcept {
    granit_shader_tools_binding_info value{};
    value.struct_size = sizeof(value);
    const auto status = granit_shader_tools_result_get_binding(handle_, index, &value);
    if (status != GRANIT_SUCCESS)
      return {::granit::from_native(status), {}};
    return {::granit::result::success,
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
  [[nodiscard]] std::pair<::granit::result, interface_variable_info>
  vertex_input(uint64_t index) const noexcept {
    return interface_variable(index, granit_shader_tools_result_get_vertex_input);
  }
  [[nodiscard]] std::pair<::granit::result, interface_variable_info>
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
  [[nodiscard]] std::pair<::granit::result, override_info>
  override_at(uint64_t index) const noexcept {
    granit_shader_tools_override_info value{};
    value.struct_size = sizeof(value);
    const auto status = granit_shader_tools_result_get_override(handle_, index, &value);
    if (status != GRANIT_SUCCESS)
      return {::granit::from_native(status), {}};
    return {::granit::result::success,
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
  [[nodiscard]] std::pair<::granit::result, bool>
  write_asset(const granit_shader_tools_asset_desc& desc) const noexcept {
    uint32_t cache_hit = 0;
    const auto status = granit_shader_tools_result_write_asset(handle_, &desc, &cache_hit);
    return {::granit::from_native(status), cache_hit != 0};
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
             value.scalar_type,
             value.bit_width,
             value.vector_size,
             {value.name, static_cast<std::size_t>(value.name_length)}}};
  }

  granit_shader_tools_result handle_ = 0;
};

class compiler {
public:
  compiler() = default;
  ~compiler() { reset(); }
  compiler(const compiler&) = delete;
  compiler& operator=(const compiler&) = delete;
  compiler(compiler&& other) noexcept : handle_(std::exchange(other.handle_, 0)) {}
  compiler& operator=(compiler&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
  }

  [[nodiscard]] ::granit::result initialize(const compiler_config& config) noexcept {
    if (handle_ != 0)
      return ::granit::result::invalid_argument;
    const granit_shader_tools_compiler_desc native{
        .struct_size = sizeof(granit_shader_tools_compiler_desc),
        .reserved = 0,
        .dxc_path = config.dxc_path.data(),
        .dxc_path_length = config.dxc_path.size(),
        .tint_path = config.tint_path.data(),
        .tint_path_length = config.tint_path.size(),
    };
    return ::granit::from_native(granit_shader_tools_compiler_create(&native, &handle_));
  }

  [[nodiscard]] std::pair<::granit::result, result>
  compile(const compile_desc& desc) const noexcept {
    if (handle_ == 0)
      return {::granit::result::invalid_handle, result{}};
    try {
      std::vector<granit_shader_tools_define> definitions;
      definitions.reserve(desc.defines.size());
      for (const auto& define : desc.defines) {
        definitions.push_back({.struct_size = sizeof(granit_shader_tools_define),
                               .reserved = 0,
                               .name = define.name.data(),
                               .name_length = define.name.size(),
                               .value = define.value.data(),
                               .value_length = define.value.size()});
      }
      const granit_shader_tools_compile_desc native{
          .struct_size = sizeof(granit_shader_tools_compile_desc),
          .source_language = static_cast<granit_shader_source_language>(desc.source_language),
          .stage = static_cast<granit_shader_stage>(desc.stage),
          .target_backends = static_cast<granit_shader_backend_flags>(desc.target_backends),
          .input_path = desc.input_path.data(),
          .input_path_length = desc.input_path.size(),
          .entry_point = desc.entry_point.data(),
          .entry_point_length = desc.entry_point.size(),
          .spirv_output_path = desc.spirv_output_path.data(),
          .spirv_output_path_length = desc.spirv_output_path.size(),
          .wgsl_output_path = desc.wgsl_output_path.data(),
          .wgsl_output_path_length = desc.wgsl_output_path.size(),
          .defines = definitions.data(),
          .define_count = static_cast<std::uint32_t>(definitions.size()),
          .validate_binding_set = desc.validate_binding_set ? 1U : 0U,
          .expected_bindings = desc.expected_bindings.data(),
          .expected_binding_count = desc.expected_bindings.size(),
      };
      granit_shader_tools_result result_handle = 0;
      const auto status = granit_shader_tools_compiler_compile(handle_, &native, &result_handle);
      return {::granit::from_native(status), result{result_handle}};
    } catch (const std::bad_alloc&) {
      return {::granit::result::out_of_memory, result{}};
    } catch (...) {
      return {::granit::result::internal, result{}};
    }
  }

  void reset() noexcept {
    if (handle_ != 0) {
      static_cast<void>(granit_shader_tools_compiler_destroy(handle_));
      handle_ = 0;
    }
  }

  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != 0; }

private:
  granit_shader_tools_compiler handle_{};
};

inline std::pair<::granit::result, std::string> tool_identity(std::string_view path) noexcept {
  uint64_t size = 0;
  auto status = granit_shader_tools_get_tool_identity(path.data(), path.size(), nullptr, &size);
  if (status != GRANIT_SUCCESS)
    return {::granit::from_native(status), {}};
  std::string identity(static_cast<std::size_t>(size), '\0');
  status = granit_shader_tools_get_tool_identity(path.data(), path.size(), identity.data(), &size);
  if (status != GRANIT_SUCCESS)
    return {::granit::from_native(status), {}};
  identity.resize(static_cast<std::size_t>(size));
  return {::granit::result::success, std::move(identity)};
}

inline std::pair<::granit::result, result>
inspect_spirv(const granit_shader_tools_inspect_desc& desc) noexcept {
  granit_shader_tools_result handle = 0;
  const auto status = granit_shader_tools_inspect_spirv(&desc, &handle);
  return {::granit::from_native(status), result{handle}};
}

inline std::pair<::granit::result, bool>
restore_asset_cache(const granit_shader_tools_cache_desc& desc) noexcept {
  uint32_t cache_hit = 0;
  const auto status = granit_shader_tools_restore_asset_cache(&desc, &cache_hit);
  return {::granit::from_native(status), cache_hit != 0};
}

inline std::pair<::granit::result, granit_shader_tools_target_capabilities>
target_capabilities(shader_backend backend,
                    shader_profile profile = shader_profile::portable) noexcept {
  granit_shader_tools_target_capabilities capabilities =
      GRANIT_SHADER_TOOLS_TARGET_CAPABILITIES_INIT;
  const auto status = granit_shader_tools_get_target_capabilities(
      static_cast<std::uint32_t>(backend), static_cast<std::uint32_t>(profile), &capabilities);
  return {::granit::from_native(status), capabilities};
}

} // namespace granit::shader_tools

#endif
