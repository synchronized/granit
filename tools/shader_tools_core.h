// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_SHADER_TOOLS_CORE_H_
#define GRANIT_TOOLS_SHADER_TOOLS_CORE_H_

#include <filesystem>
#include <iosfwd>
#include <string>

namespace granit::tools {

struct shader_info {
  std::string entry_point;
  std::string stage;
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
int compile_shader(const compile_options& options, std::ostream& output, std::ostream& error);

} // namespace granit::tools

#endif
