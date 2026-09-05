// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_SHADER_TOOLS_CORE_H_
#define GRANIT_TOOLS_SHADER_TOOLS_CORE_H_

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace granit::tools {

enum class shader_binding_type {
  uniform_buffer,
  storage_buffer,
  sampled_texture,
  storage_texture,
  sampler,
};

enum class shader_binding_access { read, write, read_write };

enum class shader_scalar_type { floating_point, signed_integer, unsigned_integer };

struct shader_interface_variable_info {
  std::uint32_t location = 0;
  std::uint32_t component = 0;
  shader_scalar_type scalar_type = shader_scalar_type::floating_point;
  std::uint32_t bit_width = 0;
  std::uint32_t vector_size = 0;
  std::string name;
};

struct shader_override_info {
  std::uint32_t id = 0;
  shader_scalar_type scalar_type = shader_scalar_type::floating_point;
  std::uint32_t bit_width = 0;
  std::string name;
  std::uint64_t default_value = 0;
  std::uint32_t default_value_size = 0;
};

struct shader_binding_info {
  std::uint32_t group = 0;
  std::uint32_t binding = 0;
  shader_binding_type type = shader_binding_type::uniform_buffer;
  shader_binding_access access = shader_binding_access::read;
  std::string name;
  std::uint32_t array_count = 0;
  std::uint64_t minimum_binding_size = 0;
};

struct shader_info {
  std::string entry_point;
  std::string stage;
  std::vector<shader_binding_info> bindings;
  std::vector<shader_interface_variable_info> vertex_inputs;
  std::vector<shader_interface_variable_info> fragment_outputs;
  std::vector<shader_override_info> overrides;
  std::uint32_t workgroup_size_x = 0;
  std::uint32_t workgroup_size_y = 0;
  std::uint32_t workgroup_size_z = 0;
};

struct compile_options {
  std::filesystem::path tint;
  std::filesystem::path input;
  std::string entry_point;
  std::string stage;
  std::filesystem::path output;
};

struct hlsl_compile_options {
  std::filesystem::path dxc;
  std::filesystem::path tint;
  std::filesystem::path input;
  std::string entry_point;
  std::string stage;
  std::filesystem::path spirv_output;
  std::filesystem::path wgsl_output;
};

bool inspect_shader(const std::filesystem::path& path, bool emit, shader_info& info,
                    std::ostream& output, std::ostream& error);
std::string serialize_shader_info_json(const shader_info& info);
int compile_shader(const compile_options& options, shader_info& info, std::ostream& output,
                   std::ostream& error);
int compile_hlsl_shader(const hlsl_compile_options& options, shader_info& info,
                        std::ostream& output, std::ostream& error);

} // namespace granit::tools

#endif
