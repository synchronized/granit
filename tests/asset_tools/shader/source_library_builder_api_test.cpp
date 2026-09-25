// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/shader_library_builder.hpp>

#include <filesystem>
#include <fstream>
#include <string>

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
  auto [status, build_result] = granit::asset_tools::shader::build_library_from_manifest(desc);
  if (status.failed() || !build_result || build_result.info().cache_hit ||
      !std::filesystem::exists(library))
    return 2;
  auto [cached_status, cached_result] =
      granit::asset_tools::shader::build_library_from_manifest(desc);
  if (cached_status.failed() || !cached_result.info().cache_hit)
    return 3;
  desc.manifest_path = "missing.grshlib.json";
  auto [failed_status, failed_result] =
      granit::asset_tools::shader::build_library_from_manifest(desc);
  if (failed_status != granit::result::invalid_argument || !failed_result ||
      failed_result.info().diagnostic.empty() || failed_result.info().cache_hit)
    return 4;

  const auto invalid_manifest = root / "invalid.grshlib.json";
  {
    std::ofstream stream{invalid_manifest, std::ios::binary};
    stream << "not-json";
  }
  const auto invalid_manifest_text = invalid_manifest.string();
  granit_asset_tools_shader_source_library_desc native =
      GRANIT_ASSET_TOOLS_SHADER_SOURCE_LIBRARY_DESC_INIT;
  native.manifest_path = invalid_manifest_text.data();
  native.manifest_path_length = invalid_manifest_text.size();
  native.toolchain_root = argv[1];
  native.toolchain_root_length = std::char_traits<char>::length(argv[1]);
  native.cache_path = cache.data();
  native.cache_path_length = cache.size();
  native.output_path = library.data();
  native.output_path_length = library.size();
  granit_asset_tools_shader_library_result native_result = 0;
  if (granit_asset_tools_shader_build_library_from_manifest(&native, &native_result) !=
          GRANIT_ERROR_INVALID_ARGUMENT ||
      native_result == 0)
    return 5;
  granit_asset_tools_shader_library_result_info native_info =
      GRANIT_ASSET_TOOLS_SHADER_LIBRARY_RESULT_INFO_INIT;
  if (granit_asset_tools_shader_library_result_get_info(native_result, &native_info) !=
          GRANIT_SUCCESS ||
      native_info.diagnostic_length == 0 ||
      granit_asset_tools_shader_library_result_destroy(native_result) != GRANIT_SUCCESS ||
      granit_asset_tools_shader_library_result_destroy(native_result) !=
          GRANIT_ERROR_INVALID_HANDLE)
    return 6;

  const auto broken_source = root / "broken.hlsl";
  const auto broken_manifest = root / "broken.grshlib.json";
  {
    std::ofstream source{broken_source, std::ios::binary};
    source << "this is not hlsl";
    std::ofstream manifest{broken_manifest, std::ios::binary};
    manifest << R"({
  "format_version": 1,
  "name": "broken",
  "target_profile": "portable",
  "target_backends": ["vulkan", "webgpu"],
  "shaders": [{
    "name": "broken.fragment",
    "source": "broken.hlsl",
    "stage": "fragment",
    "entry_point": "fragment_main"
  }]
})";
  }
  const auto broken_manifest_text = broken_manifest.string();
  desc.manifest_path = broken_manifest_text;
  auto [compile_status, compile_result] =
      granit::asset_tools::shader::build_library_from_manifest(desc);
  const auto compile_info = compile_result.info();
  if (compile_status != granit::result::initialization_failed || !compile_result ||
      compile_info.failed_shader != "broken.fragment" || compile_info.diagnostic.empty())
    return 7;
  std::filesystem::remove_all(root, error);
  return 0;
}
