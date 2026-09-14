// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_fixture_tool/commands.h"

#include <iostream>
#include <string_view>

namespace {

void print_usage() { std::cerr << "该工具只供 Granit 仓库测试生成 Shader Object 夹具。\n"; }

} // namespace

int main(int argc, char** argv) {
  using namespace granit::asset_tools::cli;
  if (argc >= 2 && std::string_view{argv[1]} == "object")
    return build_shader_fixture_object(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "library")
    return link_shader_fixture_library(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "object-ids")
    return emit_shader_fixture_object_ids(argc, argv);
  print_usage();
  return 2;
}
