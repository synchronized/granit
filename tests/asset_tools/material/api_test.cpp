// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/material_builder.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

std::string read_text(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3)
    return 1;
  const auto source = read_text(argv[1]);
  const auto index = read_text(argv[2]);
  const std::string_view indices[]{index};
  auto [build_status, built] =
      granit::asset_tools::material::build({.source_json = source, .shader_indices = indices});
  if (build_status.failed() || !built || built.archive().empty() ||
      built.debug_json().find("\"magic\": \"GRMAT\"") == std::string_view::npos ||
      !built.diagnostic().empty())
    return 2;

  auto [inspect_status, inspected] = granit::asset_tools::material::inspect(built.archive());
  if (inspect_status.failed() || !inspected ||
      !std::ranges::equal(inspected.archive(), built.archive()) ||
      inspected.debug_json() != built.debug_json())
    return 3;

  const std::string_view invalid_indices[]{"{}"};
  auto [invalid_status, invalid] = granit::asset_tools::material::build(
      {.source_json = source, .shader_indices = invalid_indices});
  if (invalid_status != granit::result::invalid_argument || !invalid ||
      invalid.diagnostic().empty() || !invalid.archive().empty())
    return 4;
  return 0;
}
