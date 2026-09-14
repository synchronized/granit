// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "granit_asset_tool/shader/arguments.h"

namespace granit::asset_tools::cli {

std::vector<std::string> option_values(int argc, char** argv, std::string_view name) {
  std::vector<std::string> values;
  for (int index = 2; index + 1 < argc; ++index) {
    if (argv[index] == name)
      values.emplace_back(argv[index + 1]);
  }
  return values;
}

std::optional<std::string> option_value(int argc, char** argv, std::string_view name) {
  for (int index = 2; index + 1 < argc; ++index) {
    if (argv[index] == name)
      return argv[index + 1];
  }
  return std::nullopt;
}

} // namespace granit::asset_tools::cli
