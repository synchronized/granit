// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_COMPILER_HPP_
#define GRANIT_SHADER_COMPILER_HPP_

#include <granit/core/result.hpp>
#include <granit/core/shader_types.hpp>
#include <granit/tools/shader_compiler.h>
#include <granit/tools/shader_reflection.hpp>

#include <cstddef>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::shader_tools {

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
  shader_stage stage{shader_stage::vertex};
  std::string_view entry_point{"main"};
  shader_backend target_backends{shader_backend::all};
  std::string_view spirv_output_path;
  std::string_view wgsl_output_path;
  std::span<const shader_define> defines;
  bool validate_binding_set{};
  std::span<const granit_shader_tools_expected_binding> expected_bindings;
};

struct compilation_info {
  ::granit::result status = ::granit::result::invalid_handle;
  std::string_view entry_point;
  shader_stage stage = shader_stage::vertex;
  std::string_view output;
  std::string_view diagnostic;
};

class compilation {
public:
  compilation() = default;
  explicit compilation(granit_shader_tools_compilation handle) noexcept : handle_(handle) {}
  ~compilation() { reset(); }
  compilation(const compilation&) = delete;
  compilation& operator=(const compilation&) = delete;
  compilation(compilation&& other) noexcept : handle_(std::exchange(other.handle_, 0)) {}
  compilation& operator=(compilation&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != 0; }
  [[nodiscard]] compilation_info info() const noexcept {
    granit_shader_tools_compilation_info value{};
    value.struct_size = sizeof(value);
    if (granit_shader_tools_compilation_get_info(handle_, &value) != GRANIT_SUCCESS)
      return {};
    return {::granit::from_native(value.status),
            {value.entry_point, static_cast<std::size_t>(value.entry_point_length)},
            static_cast<shader_stage>(value.stage),
            {value.output, static_cast<std::size_t>(value.output_length)},
            {value.diagnostic, static_cast<std::size_t>(value.diagnostic_length)}};
  }
  [[nodiscard]] std::pair<::granit::result, class reflection> reflection() const noexcept {
    granit_shader_tools_reflection handle = 0;
    const auto status = granit_shader_tools_compilation_get_reflection(handle_, &handle);
    return {::granit::from_native(status), class reflection{handle}};
  }
  [[nodiscard]] std::span<const std::byte> spirv() const noexcept {
    const void* data = nullptr;
    uint64_t size = 0;
    if (granit_shader_tools_compilation_get_spirv(handle_, &data, &size) != GRANIT_SUCCESS)
      return {};
    return {static_cast<const std::byte*>(data), static_cast<std::size_t>(size)};
  }
  [[nodiscard]] std::string_view wgsl() const noexcept {
    const char* source = nullptr;
    uint64_t length = 0;
    if (granit_shader_tools_compilation_get_wgsl(handle_, &source, &length) != GRANIT_SUCCESS)
      return {};
    return {source, static_cast<std::size_t>(length)};
  }
  void reset() noexcept {
    if (handle_ != 0) {
      static_cast<void>(granit_shader_tools_compilation_destroy(handle_));
      handle_ = 0;
    }
  }

private:
  granit_shader_tools_compilation handle_ = 0;
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

  [[nodiscard]] std::pair<::granit::result, compilation>
  compile(const compile_desc& desc) const noexcept {
    if (handle_ == 0)
      return {::granit::result::invalid_handle, compilation{}};
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
      granit_shader_tools_compilation compilation_handle = 0;
      const auto status =
          granit_shader_tools_compiler_compile(handle_, &native, &compilation_handle);
      return {::granit::from_native(status), compilation{compilation_handle}};
    } catch (const std::bad_alloc&) {
      return {::granit::result::out_of_memory, compilation{}};
    } catch (...) {
      return {::granit::result::internal, compilation{}};
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
