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
  auto [target_status, target_capabilities] =
      granit::shader_tools::target_capabilities(granit::shader_backend::vulkan);
  if (target_status.failed() || target_capabilities.backend != GRANIT_SHADER_BACKEND_VULKAN_BIT ||
      target_capabilities.profile != GRANIT_SHADER_PROFILE_PORTABLE ||
      target_capabilities.supported_features != 0)
    return 14;
  std::tie(target_status, target_capabilities) =
      granit::shader_tools::target_capabilities(granit::shader_backend::all);
  if (target_status != granit::result::unsupported)
    return 15;
  granit_shader_tools_inspect_desc inspect{};
  inspect.struct_size = sizeof(inspect);
  inspect.input_path = argv[1];
  inspect.input_path_length = std::strlen(argv[1]);
  auto [inspect_status, reflection] = granit::shader_tools::inspect_spirv(inspect);
  if (inspect_status.failed() || !reflection)
    return 2;

  const std::string output = (std::filesystem::path{argv[3]} / "fixture.grshaderobj").string();
  const std::string restored = (std::filesystem::path{argv[3]} / "restored.spv").string();
  std::error_code error;
  std::filesystem::remove_all(argv[3], error);
  constexpr std::string_view revision = "dawn-v20260720.160313";
  constexpr std::string_view target = "vulkan1.3";
  std::string options = "format=spirv;validate=1";
  granit_shader_tools_object_desc object{};
  object.struct_size = sizeof(object);
  object.source_path = argv[2];
  object.source_path_length = std::strlen(argv[2]);
  object.wgsl_path = argv[2];
  object.wgsl_path_length = std::strlen(argv[2]);
  object.spirv_path = argv[1];
  object.spirv_path_length = std::strlen(argv[1]);
  object.output_path = output.data();
  object.output_path_length = output.size();
  object.tint_revision = revision.data();
  object.tint_revision_length = revision.size();
  object.target_environment = target.data();
  object.target_environment_length = target.size();
  object.compile_options = options.data();
  object.compile_options_length = options.size();
  object.backend_mask = GRANIT_SHADER_BACKEND_ALL_BITS;
  object.required_features = GRANIT_SHADER_FEATURE_FLOAT16_BIT;
  object.entry_point = "main";
  object.entry_point_length = 4;
  object.stage = GRANIT_SHADER_STAGE_COMPUTE;
  auto [unsupported_status, unsupported_hit] = granit::shader_tools::build_object(object);
  if (unsupported_status != granit::result::unsupported || unsupported_hit)
    return 16;
  object.required_features = 0;

  auto [status, cache_hit] = granit::shader_tools::build_object(object);
  if (status.failed() || cache_hit)
    return 3;
  const auto first = std::filesystem::file_size(output, error);
  if (error || first == 0)
    return 4;
  if (!std::filesystem::exists(output + ".wgsl") || !std::filesystem::exists(output + ".spv"))
    return 41;
  granit_shader_tools_object_cache_desc cache{};
  cache.struct_size = sizeof(cache);
  cache.source_path = argv[2];
  cache.source_path_length = std::strlen(argv[2]);
  cache.spirv_output_path = restored.data();
  cache.spirv_output_path_length = restored.size();
  cache.object_path = output.data();
  cache.object_path_length = output.size();
  cache.entry_point = "main";
  cache.entry_point_length = 4;
  cache.stage = GRANIT_SHADER_STAGE_COMPUTE;
  cache.tint_revision = revision.data();
  cache.tint_revision_length = revision.size();
  cache.target_environment = target.data();
  cache.target_environment_length = target.size();
  cache.compile_options = options.data();
  cache.compile_options_length = options.size();
  cache.backend_mask = GRANIT_SHADER_BACKEND_ALL_BITS;
  cache.required_features = 0;
  auto [restore_status, restored_hit] = granit::shader_tools::restore_object_cache(cache);
  if (restore_status.failed())
    return 51;
  if (!restored_hit)
    return 52;
  if (std::filesystem::file_size(restored, error) != std::filesystem::file_size(argv[1], error))
    return 53;
  std::tie(status, cache_hit) = granit::shader_tools::build_object(object);
  if (status.failed() || !cache_hit)
    return 6;
  options += ";robustness=1";
  object.compile_options = options.data();
  object.compile_options_length = options.size();
  std::tie(status, cache_hit) = granit::shader_tools::build_object(object);
  if (status.failed() || cache_hit || std::filesystem::file_size(output, error) != first)
    return 7;
  {
    auto stream = std::ofstream{output + ".spv", std::ios::binary | std::ios::trunc};
    stream.put('\0');
  }
  std::filesystem::remove(restored, error);
  cache.compile_options = options.data();
  cache.compile_options_length = options.size();
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_object_cache(cache);
  if (restore_status.failed() || restored_hit)
    return 71;
  std::tie(status, cache_hit) = granit::shader_tools::build_object(object);
  if (status.failed() || cache_hit)
    return 72;
  std::filesystem::remove(output + ".wgsl", error);
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_object_cache(cache);
  if (restore_status.failed() || restored_hit)
    return 73;
  std::tie(status, cache_hit) = granit::shader_tools::build_object(object);
  if (status.failed() || cache_hit)
    return 74;
  std::filesystem::remove(restored, error);
  cache.compile_options = options.data();
  cache.compile_options_length = options.size();
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_object_cache(cache);
  if (restore_status.failed() || !restored_hit ||
      std::filesystem::file_size(restored, error) != std::filesystem::file_size(argv[1], error))
    return 8;
  options += ";changed=1";
  cache.compile_options = options.data();
  cache.compile_options_length = options.size();
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_object_cache(cache);
  if (restore_status.failed() || restored_hit)
    return 9;
  object.compile_options = options.data();
  object.compile_options_length = options.size();
  object.backend_mask = GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  cache.backend_mask = GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  std::tie(status, cache_hit) = granit::shader_tools::build_object(object);
  if (status.failed() || cache_hit || !std::filesystem::exists(output + ".wgsl") ||
      std::filesystem::exists(output + ".spv"))
    return 10;
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_object_cache(cache);
  if (restore_status.failed() || restored_hit)
    return 11;
  object.backend_mask = GRANIT_SHADER_BACKEND_VULKAN_BIT;
  cache.backend_mask = GRANIT_SHADER_BACKEND_VULKAN_BIT;
  std::tie(status, cache_hit) = granit::shader_tools::build_object(object);
  if (status.failed() || cache_hit || std::filesystem::exists(output + ".wgsl") ||
      !std::filesystem::exists(output + ".spv"))
    return 12;
  std::tie(restore_status, restored_hit) = granit::shader_tools::restore_object_cache(cache);
  if (restore_status.failed() || !restored_hit)
    return 13;
  std::filesystem::remove_all(argv[3], error);
  return 0;
}
