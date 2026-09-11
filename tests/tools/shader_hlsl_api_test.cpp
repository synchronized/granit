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
  granit::shader_tools::compiler compiler;
  if (compiler.initialize({dxc, tint}).failed())
    return 3;
  const granit::shader_tools::shader_define definitions[]{{.name = "TEST_VALUE", .value = "1"}};
  granit::shader_tools::compile_desc desc;
  desc.input_path = input;
  desc.source_language = granit::shader_source_language::hlsl;
  desc.stage = granit::shader_stage::fragment;
  desc.entry_point = "fragment_main";
  desc.spirv_output_path = spirv;
  desc.wgsl_output_path = wgsl;
  desc.defines = definitions;

  auto [status, result] = compiler.compile(desc);
  auto [reflection_status, reflection] = result.reflection();
  if (status.failed() || reflection_status.failed() ||
      result.info().stage != granit::shader_stage::fragment || reflection.binding_count() != 3 ||
      reflection.override_count() != 1 || result.spirv().empty() || result.wgsl().empty() ||
      !std::filesystem::exists(spirv) || !std::filesystem::exists(wgsl) ||
      read_spirv_version(spirv) < UINT32_C(0x00010600) ||
      std::filesystem::exists(spirv + ".tint-input.spv") ||
      read_text(wgsl).find("@fragment") == std::string::npos)
    return 3;
  result.reset();
  if (!reflection || reflection.binding_count() != 3)
    return 6;

  const granit::shader_tools::shader_define duplicate_definitions[]{definitions[0], definitions[0]};
  desc.defines = duplicate_definitions;
  auto [duplicate_status, duplicate_result] = compiler.compile(desc);
  if (duplicate_status != granit::result::invalid_argument || duplicate_result)
    return 4;

  desc.defines = definitions;
  desc.stage = static_cast<granit::shader_stage>(0);
  auto [invalid_status, invalid_result] = compiler.compile(desc);
  return invalid_status == granit::result::invalid_argument && !invalid_result ? 0 : 5;
}
