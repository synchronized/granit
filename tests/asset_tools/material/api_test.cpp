// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/material_builder.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string read_text(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(bytes.size());
  std::ranges::copy(std::as_bytes(std::span{bytes}), result.begin());
  return result;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3)
    return 1;
  const auto source = read_text(argv[1]);
  const auto library = read_bytes(argv[2]);
  const std::span<const std::byte> libraries[]{library};
  auto [build_status, built] =
      granit::asset_tools::material::build({.source_json = source, .shader_libraries = libraries});
  const auto built_info = built.info();
  if (build_status.failed() || !built || built_info.archive.empty() ||
      built_info.debug_json.find("\"magic\": \"GRMAT\"") == std::string_view::npos ||
      !built_info.diagnostic.empty())
    return 2;

  auto [inspect_status, inspected] = granit::asset_tools::material::inspect(built.archive());
  if (inspect_status.failed() || !inspected ||
      !std::ranges::equal(inspected.archive(), built.archive()) ||
      inspected.debug_json() != built.debug_json())
    return 3;

  const std::array invalid_archive{std::byte{0}};
  const std::span<const std::byte> invalid_libraries[]{invalid_archive};
  auto [invalid_status, invalid] = granit::asset_tools::material::build(
      {.source_json = source, .shader_libraries = invalid_libraries});
  if (invalid_status != granit::result::invalid_argument || !invalid ||
      invalid.info().diagnostic.empty() || !invalid.info().archive.empty())
    return 4;
  return 0;
}
