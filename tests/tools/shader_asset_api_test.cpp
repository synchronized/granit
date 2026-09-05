// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
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
  if (inspect_status.failed())
    return 2;

  const std::string output = (std::filesystem::path{argv[3]} / "fixture.granit-shader").string();
  const std::string restored = (std::filesystem::path{argv[3]} / "restored.spv").string();
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
  asset.backend_mask = GRANIT_SHADER_TOOLS_ASSET_BACKEND_ALL;

  auto [status, cache_hit] = result.write_asset(asset);
  if (status.failed() || cache_hit)
    return 3;
  const auto first = std::filesystem::file_size(output, error);
  if (error || first == 0)
    return 4;
  if (!std::filesystem::exists(output + ".wgsl") || !std::filesystem::exists(output + ".spv"))
    return 41;
  granit_shader_tools_cache_desc cache{};
  cache.struct_size = sizeof(cache);
  cache.wgsl_path = argv[2];
  cache.wgsl_path_length = std::strlen(argv[2]);
  cache.spirv_output_path = restored.data();
  cache.spirv_output_path_length = restored.size();
  cache.asset_path = output.data();
  cache.asset_path_length = output.size();
  cache.entry_point = "main";
  cache.entry_point_length = 4;
  cache.stage = GRANIT_SHADER_TOOLS_STAGE_COMPUTE;
  cache.tint_revision = revision.data();
  cache.tint_revision_length = revision.size();
  cache.target_environment = target.data();
  cache.target_environment_length = target.size();
  cache.compile_options = options.data();
  cache.compile_options_length = options.size();
  cache.backend_mask = GRANIT_SHADER_TOOLS_ASSET_BACKEND_ALL;
  auto [restore_status, restored_hit] = granit::shader_tools::restore_asset_cache(cache);
  if (restore_status.failed())
    return 51;
  if (!restored_hit)
    return 52;
  if (std::filesystem::file_size(restored, error) != std::filesystem::file_size(argv[1], error))
    return 53;
  std::tie(status, cache_hit) = result.write_asset(asset);
  if (status.failed() || !cache_hit)
    return 6;
  options += ";robustness=1";
  asset.compile_options = options.data();
  asset.compile_options_length = options.size();
  std::tie(status, cache_hit) = result.write_asset(asset);
  if (status.failed() || cache_hit || std::filesystem::file_size(output, error) != first)
    return 7;
  {
    auto stream = std::ofstream{output + ".spv", std::ios::binary | std::ios::trunc};
    stream.put('\0');
  }
  std::filesystem::remove(restored, error);
  cache.compile_options = options.data();
  cache.compile_options_length = options.size();
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_asset_cache(cache);
  if (restore_status.failed() || restored_hit)
    return 71;
  std::tie(status, cache_hit) = result.write_asset(asset);
  if (status.failed() || cache_hit)
    return 72;
  std::filesystem::remove(output + ".wgsl", error);
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_asset_cache(cache);
  if (restore_status.failed() || restored_hit)
    return 73;
  std::tie(status, cache_hit) = result.write_asset(asset);
  if (status.failed() || cache_hit)
    return 74;
  std::filesystem::remove(restored, error);
  cache.compile_options = options.data();
  cache.compile_options_length = options.size();
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_asset_cache(cache);
  if (restore_status.failed() || !restored_hit ||
      std::filesystem::file_size(restored, error) != std::filesystem::file_size(argv[1], error))
    return 8;
  options += ";changed=1";
  cache.compile_options = options.data();
  cache.compile_options_length = options.size();
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_asset_cache(cache);
  if (restore_status.failed() || restored_hit)
    return 9;
  asset.compile_options = options.data();
  asset.compile_options_length = options.size();
  asset.backend_mask = GRANIT_SHADER_TOOLS_ASSET_BACKEND_WEBGPU;
  cache.backend_mask = GRANIT_SHADER_TOOLS_ASSET_BACKEND_WEBGPU;
  std::tie(status, cache_hit) = result.write_asset(asset);
  if (status.failed() || cache_hit || !std::filesystem::exists(output + ".wgsl") ||
      std::filesystem::exists(output + ".spv"))
    return 10;
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_asset_cache(cache);
  if (restore_status.failed() || restored_hit)
    return 11;
  asset.backend_mask = GRANIT_SHADER_TOOLS_ASSET_BACKEND_VULKAN;
  cache.backend_mask = GRANIT_SHADER_TOOLS_ASSET_BACKEND_VULKAN;
  std::tie(status, cache_hit) = result.write_asset(asset);
  if (status.failed() || cache_hit || std::filesystem::exists(output + ".wgsl") ||
      !std::filesystem::exists(output + ".spv"))
    return 12;
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_asset_cache(cache);
  if (restore_status.failed() || !restored_hit)
    return 13;
  std::filesystem::remove_all(argv[3], error);
  return 0;
}
