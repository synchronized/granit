// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_CLI_ARGUMENTS_H_
#define GRANIT_SHADER_CLI_ARGUMENTS_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace granit::shader_cli {

std::optional<std::string> option_value(int argc, char** argv, std::string_view name);
std::vector<std::string> option_values(int argc, char** argv, std::string_view name);

} // namespace granit::shader_cli

#endif
