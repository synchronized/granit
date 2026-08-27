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
};

struct compile_options {
  std::filesystem::path tint;
  std::filesystem::path input;
  std::string entry_point;
  std::string stage;
  std::filesystem::path output;
};

bool inspect_shader(const std::filesystem::path& path, bool emit, shader_info& info,
                    std::ostream& output, std::ostream& error);
int compile_shader(const compile_options& options, shader_info& info, std::ostream& output,
                   std::ostream& error);

} // namespace granit::tools

#endif
