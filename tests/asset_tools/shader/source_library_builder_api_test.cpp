// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/shader_library_builder.hpp>

#include <filesystem>
#include <tuple>

int main(int argc, char** argv) {
  if (argc != 4)
    return 1;
  const std::filesystem::path root{argv[3]};
  const auto cache = (root / "objects").string();
  const auto library = (root / "fixture.grshlib").string();
  std::error_code error;
  std::filesystem::remove_all(root, error);
  granit::asset_tools::shader::source_library_desc desc{
      .manifest_path = argv[2],
      .toolchain_root = argv[1],
      .cache_path = cache,
      .output_path = library,
  };
  auto [status, cache_hit] = granit::asset_tools::shader::build_library_from_manifest(desc);
  if (status.failed() || cache_hit || !std::filesystem::exists(library))
    return 2;
  std::tie(status, cache_hit) = granit::asset_tools::shader::build_library_from_manifest(desc);
  if (status.failed() || !cache_hit)
    return 3;
  desc.manifest_path = "missing.grshlib.json";
  std::tie(status, cache_hit) = granit::asset_tools::shader::build_library_from_manifest(desc);
  if (status != granit::result::invalid_argument || cache_hit)
    return 4;
  std::filesystem::remove_all(root, error);
  return 0;
}
