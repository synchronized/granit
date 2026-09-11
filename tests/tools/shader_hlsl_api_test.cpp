// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string read_text(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

std::uint32_t read_spirv_version(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  std::uint32_t words[2]{};
  stream.read(reinterpret_cast<char*>(words), sizeof(words));
  return stream ? words[1] : 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 5)
    return 1;
  const std::filesystem::path output_directory = argv[4];
  std::error_code error;
  std::filesystem::create_directories(output_directory, error);
  if (error)
    return 2;
  const auto spirv = (output_directory / "material.spv").string();
  const auto wgsl = (output_directory / "material.wgsl").string();
  const std::string dxc = argv[1];
  const std::string tint = argv[2];
  const std::string input = argv[3];
  granit_shader_tools_hlsl_compile_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.dxc_path = dxc.data();
  desc.dxc_path_length = dxc.size();
  desc.tint_path = tint.data();
  desc.tint_path_length = tint.size();
  desc.input_path = input.data();
  desc.input_path_length = input.size();
  desc.entry_point = "fragment_main";
  desc.entry_point_length = 13;
  desc.stage = GRANIT_SHADER_STAGE_FRAGMENT;
  desc.spirv_output_path = spirv.data();
  desc.spirv_output_path_length = spirv.size();
  desc.wgsl_output_path = wgsl.data();
  desc.wgsl_output_path_length = wgsl.size();
  const granit_shader_tools_define definitions[]{{.struct_size = sizeof(granit_shader_tools_define),
                                                  .reserved = 0,
                                                  .name = "TEST_VALUE",
                                                  .name_length = 10,
                                                  .value = "1",
                                                  .value_length = 1}};
  desc.defines = definitions;
  desc.define_count = 1;

  auto [status, result] = granit::shader_tools::compile_hlsl(desc);
  if (status.failed() || result.info().stage != granit::shader_stage::fragment ||
      result.binding_count() != 3 || result.override_count() != 1 ||
      !std::filesystem::exists(spirv) || !std::filesystem::exists(wgsl) ||
      read_spirv_version(spirv) < UINT32_C(0x00010600) ||
      std::filesystem::exists(spirv + ".tint-input.spv") ||
      read_text(wgsl).find("@fragment") == std::string::npos)
    return 3;

  const granit_shader_tools_define duplicate_definitions[]{definitions[0], definitions[0]};
  desc.defines = duplicate_definitions;
  desc.define_count = 2;
  auto [duplicate_status, duplicate_result] = granit::shader_tools::compile_hlsl(desc);
  if (duplicate_status != granit::result::invalid_argument || duplicate_result)
    return 4;

  desc.defines = definitions;
  desc.define_count = 1;
  desc.stage = 0;
  auto [invalid_status, invalid_result] = granit::shader_tools::compile_hlsl(desc);
  return invalid_status == granit::result::invalid_argument && !invalid_result ? 0 : 5;
}
