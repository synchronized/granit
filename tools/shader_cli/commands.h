// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_CLI_COMMANDS_H_
#define GRANIT_SHADER_CLI_COMMANDS_H_

#include <granit/core/shader_types.hpp>

namespace granit::shader_cli {

int link_shader_library(int argc, char** argv);
int build_shader_library(int argc, char** argv);
int build_shader_object(int argc, char** argv);
int emit_shader_object_ids(int argc, char** argv);
int emit_shader_index_ids(int argc, char** argv);
int compile_shader(int argc, char** argv);
int inspect_shader(const char* path, bool verify, bool json = false);
int print_target_capabilities(shader_backend backend);

} // namespace granit::shader_cli

#endif
