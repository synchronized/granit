// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.hpp>

#include <cstring>
#include <string_view>

int main(int argc, char** argv) {
  if (argc != 4 && argc != 5)
    return 1;
  granit_shader_tools_inspect_desc desc{};
  constexpr auto expected_size =
      static_cast<uint32_t>(sizeof(granit_shader_tools_expected_binding));
  granit_shader_tools_expected_binding expected[]{
      {expected_size, 0, 0}, {expected_size, 0, 1}, {expected_size, 0, 2}};
  desc.struct_size = sizeof(desc);
  desc.input_path = argv[1];
  desc.input_path_length = std::strlen(argv[1]);
  desc.validate_binding_set = 1;
  desc.expected_bindings = expected;
  desc.expected_binding_count = sizeof(expected) / sizeof(expected[0]);
  auto [status, result] = granit::shader_tools::inspect_spirv(desc);
  if (status != GRANIT_SUCCESS || !result)
    return 2;
  const auto info = result.info();
  if (info.status != GRANIT_SUCCESS || info.stage != GRANIT_SHADER_TOOLS_STAGE_FRAGMENT ||
      info.entry_point.empty() || info.output.find("schema,1") == std::string_view::npos)
    return 3;
  if (result.binding_count() != 3)
    return 4;
  const auto [binding_status, binding] = result.binding(0);
  if (binding_status != GRANIT_SUCCESS || binding.group != 0 || binding.binding != 0 ||
      binding.type != GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER ||
      binding.access != GRANIT_SHADER_TOOLS_ACCESS_READ || binding.minimum_binding_size != 16 ||
      binding.name.empty())
    return 5;
  if (result.fragment_output_count() != 1)
    return 6;
  const auto [output_status, output] = result.fragment_output(0);
  if (output_status != GRANIT_SUCCESS || output.location != 0 ||
      output.scalar_type != GRANIT_SHADER_TOOLS_SCALAR_FLOAT || output.bit_width != 32 ||
      output.vector_size != 4)
    return 7;
  auto moved = std::move(result);
  if (!moved || result)
    return 8;

  desc.input_path = argv[2];
  desc.input_path_length = std::strlen(argv[2]);
  desc.validate_binding_set = 0;
  auto [vertex_status, vertex_result] = granit::shader_tools::inspect_spirv(desc);
  if (vertex_status != GRANIT_SUCCESS || vertex_result.vertex_input_count() == 0)
    return 9;
  const auto [input_status, input] = vertex_result.vertex_input(0);
  if (input_status != GRANIT_SUCCESS || input.location != 0 || input.component != 0 ||
      input.bit_width != 32 || input.vector_size == 0)
    return 10;

  desc.input_path = argv[3];
  desc.input_path_length = std::strlen(argv[3]);
  auto [compute_status, compute_result] = granit::shader_tools::inspect_spirv(desc);
  const auto workgroup = compute_result.compute_workgroup_size();
  if (compute_status != GRANIT_SUCCESS || workgroup.x == 0 || workgroup.y == 0 || workgroup.z == 0)
    return 11;
  if (argc == 5) {
    desc.input_path = argv[4];
    desc.input_path_length = std::strlen(argv[4]);
    auto [override_status, override_result] = granit::shader_tools::inspect_spirv(desc);
    if (override_status != GRANIT_SUCCESS || override_result.override_count() != 1)
      return 12;
    const auto [constant_status, constant] = override_result.override_at(0);
    if (constant_status != GRANIT_SUCCESS || constant.id != 7 ||
        constant.scalar_type != GRANIT_SHADER_TOOLS_SCALAR_FLOAT || constant.bit_width != 32 ||
        constant.default_value_size != 4)
      return 13;
  }
  return 0;
}
