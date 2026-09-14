// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "granit_asset_tool/environment/commands.h"
#include "granit_asset_tool/material/commands.h"
#include "granit_asset_tool/shader/commands.h"
#include "granit_asset_tool/texture/commands.h"

#include <iostream>
#include <string_view>

namespace {

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_asset_tool shader inspect <shader.spv>\n"
               "  granit_asset_tool shader inspect --json <shader.spv>\n"
               "  granit_asset_tool shader verify <shader.spv>\n"
               "  granit_asset_tool shader targets\n"
               "  granit_asset_tool shader capabilities "
               "--target <vulkan-portable|webgpu-portable>\n"
               "  granit_asset_tool shader build-library --manifest <library.grshlib.json> "
               "--toolchain <root> --cache <directory> --output <library.grshlib> "
               "--index <library.grshidx.json>\n"
               "  granit_asset_tool shader index-ids --index <library.grshidx.json> "
               "--shader <name=logical-name>... --output <shader-ids.inc>\n"
               "  granit_asset_tool shader compile --toolchain <root> "
               "--input <shader.hlsl> --entry <name> --stage <vertex|fragment|compute> "
               "--spirv-output <shader.spv> --wgsl-output <shader.wgsl> "
               "[--define <name=value>]...\n"
               "  granit_asset_tool material build <source.grmat.json> ...\n"
               "  granit_asset_tool material inspect <package.grmat> --json ...\n"
               "  granit_asset_tool texture build ...\n"
               "  granit_asset_tool texture inspect <manifest.grtex> --json ...\n"
               "  granit_asset_tool environment build ...\n"
               "  granit_asset_tool environment inspect <environment.grenv> --json ...\n";
}

} // namespace

int run_shader_command(int argc, char** argv) {
  using namespace granit::asset_tools::cli;
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
  if (argc >= 2 && std::string_view{argv[1]} == "build-library")
    return build_shader_library(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "index-ids")
    return emit_shader_index_ids(argc, argv);
  print_usage();
  return 2;
}

int main(int argc, char** argv) {
  using namespace granit::asset_tools::cli;
  if (argc >= 2 && std::string_view{argv[1]} == "shader")
    return run_shader_command(argc - 1, argv + 1);
  if (argc >= 2 && std::string_view{argv[1]} == "material")
    return run_material_command(argc - 1, argv + 1);
  if (argc >= 2 && std::string_view{argv[1]} == "texture")
    return run_texture_command(argc - 1, argv + 1);
  if (argc >= 2 && std::string_view{argv[1]} == "environment")
    return run_environment_command(argc - 1, argv + 1);
  print_usage();
  return 2;
}
