// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.hpp>

#include <cstring>
#include <string_view>

int main(int argc, char** argv) {
  if (argc != 2)
    return 1;
  granit_shader_tools_inspect_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.input_path = argv[1];
  desc.input_path_length = std::strlen(argv[1]);
  auto [status, result] = granit::shader_tools::inspect_spirv(desc);
  if (status != GRANIT_SUCCESS || !result)
    return 2;
  const auto info = result.info();
  if (info.status != GRANIT_SUCCESS || info.stage != GRANIT_SHADER_TOOLS_STAGE_FRAGMENT ||
      info.entry_point.empty() || info.output.find("schema,1") == std::string_view::npos)
    return 3;
  auto moved = std::move(result);
  return moved && !result ? 0 : 4;
}
