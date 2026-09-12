// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/commands.h"

#include <iostream>
#include <string_view>

namespace {

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_shader_tool inspect <shader.spv>\n"
               "  granit_shader_tool inspect --json <shader.spv>\n"
               "  granit_shader_tool verify <shader.spv>\n"
               "  granit_shader_tool targets\n"
               "  granit_shader_tool capabilities --target <vulkan-portable|webgpu-portable>\n"
               "  granit_shader_tool library --object <shader.grshaderobj>... "
               "--target <all|vulkan|webgpu> --output <shaders.grshlib>\n"
               "  granit_shader_tool build-library --manifest <library.grshlib.json> "
               "--dxc <path> --tint <path> --cache <directory> --output <library.grshlib> "
               "--index <library.grshidx.json>\n"
               "  granit_shader_tool object-ids --object <name=shader.grshaderobj>... "
               "--output <shader-ids.inc>\n"
               "  granit_shader_tool index-ids --index <library.grshidx.json> "
               "--shader <name=logical-name>... --output <shader-ids.inc>\n"
               "  granit_shader_tool compile --dxc <path> --tint <path> "
               "--input <shader.hlsl> --entry <name> --stage <vertex|fragment|compute> "
               "--spirv-output <shader.spv> --wgsl-output <shader.wgsl> "
               "[--define <NAME=VALUE>]... "
               "[--object <shader.grshaderobj> [--dxc-revision <revision>] "
               "[--tint-revision <revision>] --object-backend <all|vulkan|webgpu>]\n";
}

} // namespace

int main(int argc, char** argv) {
  using namespace granit::shader_cli;
  if (argc == 2 && std::string_view{argv[1]} == "targets") {
    std::cout << "vulkan-portable\nwebgpu-portable\n";
    return 0;
  }
  if (argc == 4 && std::string_view{argv[1]} == "capabilities" &&
      std::string_view{argv[2]} == "--target") {
    const std::string_view target{argv[3]};
    if (target == "vulkan-portable")
      return print_target_capabilities(granit::shader_backend::vulkan);
    if (target == "webgpu-portable")
      return print_target_capabilities(granit::shader_backend::webgpu);
  }
  if (argc == 3 && std::string_view{argv[1]} == "inspect")
    return inspect_shader(argv[2], false);
  if (argc == 4 && std::string_view{argv[1]} == "inspect" && std::string_view{argv[2]} == "--json")
    return inspect_shader(argv[3], false, true);
  if (argc == 3 && std::string_view{argv[1]} == "verify")
    return inspect_shader(argv[2], true);
  if (argc >= 2 && std::string_view{argv[1]} == "compile")
    return compile_shader(argc, argv);
  // 仅供无 HLSL 工具链的仓库测试封装已检入夹具，不作为作者入口公开。
  if (argc >= 2 && std::string_view{argv[1]} == "fixture-object")
    return build_shader_fixture_object(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "library")
    return link_shader_library(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "build-library")
    return build_shader_library(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "object-ids")
    return emit_shader_object_ids(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "index-ids")
    return emit_shader_index_ids(argc, argv);
  print_usage();
  return 2;
}
