// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.hpp>

#include <cstring>
#include <filesystem>
#include <string>
#include <tuple>

int main(int argc, char** argv) {
  if (argc != 4)
    return 1;
  granit_shader_tools_inspect_desc inspect{};
  inspect.struct_size = sizeof(inspect);
  inspect.input_path = argv[1];
  inspect.input_path_length = std::strlen(argv[1]);
  auto [inspect_status, result] = granit::shader_tools::inspect_spirv(inspect);
  if (inspect_status != GRANIT_SUCCESS)
    return 2;

  const std::string output = (std::filesystem::path{argv[3]} / "fixture.granit-shader").string();
  std::error_code error;
  std::filesystem::remove_all(argv[3], error);
  constexpr std::string_view revision = "dawn-v20260720.160313";
  constexpr std::string_view target = "vulkan1.3";
  std::string options = "format=spirv;validate=1";
  granit_shader_tools_asset_desc asset{};
  asset.struct_size = sizeof(asset);
  asset.wgsl_path = argv[2];
  asset.wgsl_path_length = std::strlen(argv[2]);
  asset.spirv_path = argv[1];
  asset.spirv_path_length = std::strlen(argv[1]);
  asset.output_path = output.data();
  asset.output_path_length = output.size();
  asset.tint_revision = revision.data();
  asset.tint_revision_length = revision.size();
  asset.target_environment = target.data();
  asset.target_environment_length = target.size();
  asset.compile_options = options.data();
  asset.compile_options_length = options.size();

  auto [status, cache_hit] = result.write_asset(asset);
  if (status != GRANIT_SUCCESS || cache_hit)
    return 3;
  const auto first = std::filesystem::file_size(output, error);
  if (error || first == 0)
    return 4;
  std::tie(status, cache_hit) = result.write_asset(asset);
  if (status != GRANIT_SUCCESS || !cache_hit)
    return 5;
  options += ";robustness=1";
  asset.compile_options_length = options.size();
  std::tie(status, cache_hit) = result.write_asset(asset);
  if (status != GRANIT_SUCCESS || cache_hit || std::filesystem::file_size(output, error) != first)
    return 6;
  std::filesystem::remove_all(argv[3], error);
  return 0;
}
