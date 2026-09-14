// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material_cli/commands.h"

#include "asset_file_io.h"

#include <granit/asset_tools/material_builder.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_asset_tool material build <source.grmat.json> "
               "--output <package.grmat> --shader-index <library.grshidx.json>... "
               "[--emit-debug-json [debug.json]]\n"
               "  granit_asset_tool material inspect <package.grmat> --json "
               "[--output <debug.json>]\n";
}

int inspect_package(int argc, char** argv) {
  if ((argc != 4 && argc != 6) || std::string_view{argv[3]} != "--json" ||
      (argc == 6 && std::string_view{argv[4]} != "--output")) {
    print_usage();
    return 2;
  }
  const auto bytes = granit::asset_tools::cli::read_file(argv[2]);
  if (bytes.empty()) {
    std::cerr << "无法读取材质包\n";
    return 1;
  }
  auto [status, result] = granit::asset_tools::material::inspect(bytes);
  if (status.failed()) {
    std::cerr << result.diagnostic();
    return 1;
  }
  if (argc == 6) {
    if (!granit::asset_tools::cli::write_file_atomic(argv[5], result.debug_json())) {
      std::cerr << "无法原子写入调试 JSON\n";
      return 1;
    }
  } else {
    std::cout << result.debug_json();
  }
  return 0;
}

int build_package(int argc, char** argv) {
  if (argc < 7 || std::string_view{argv[3]} != "--output") {
    print_usage();
    return 2;
  }
  bool emit_debug_json = false;
  std::filesystem::path debug_path;
  std::vector<std::string> index_json;
  for (int index = 5; index < argc;) {
    const std::string_view option{argv[index]};
    if (option == "--shader-index" && index + 1 < argc) {
      auto json = granit::asset_tools::cli::read_text_file(argv[index + 1]);
      if (json.empty()) {
        std::cerr << "无法读取 Shader Library 索引\n";
        return 1;
      }
      index_json.push_back(std::move(json));
      index += 2;
    } else if (option == "--emit-debug-json") {
      if (emit_debug_json) {
        print_usage();
        return 2;
      }
      emit_debug_json = true;
      ++index;
      if (index < argc && std::string_view{argv[index]}.find("--") != 0)
        debug_path = argv[index++];
    } else {
      print_usage();
      return 2;
    }
  }
  if (index_json.empty()) {
    print_usage();
    return 2;
  }
  const auto source = granit::asset_tools::cli::read_text_file(argv[2]);
  if (source.empty()) {
    std::cerr << "无法读取材质源描述\n";
    return 1;
  }
  std::vector<std::string_view> index_views;
  index_views.reserve(index_json.size());
  for (const auto& json : index_json)
    index_views.push_back(json);
  auto [status, result] = granit::asset_tools::material::build({source, index_views});
  if (status.failed()) {
    std::cerr << result.diagnostic();
    return 1;
  }
  const std::filesystem::path output_path = argv[4];
  if (!granit::asset_tools::cli::write_file_atomic(output_path, result.archive())) {
    std::cerr << "无法原子写入材质包\n";
    return 1;
  }
  if (emit_debug_json) {
    if (debug_path.empty())
      debug_path = std::filesystem::path{output_path.string() + ".debug.json"};
    if (!granit::asset_tools::cli::write_file_atomic(debug_path, result.debug_json())) {
      std::cerr << "材质包已生成，但无法导出调试 JSON\n";
      return 1;
    }
  }
  return 0;
}

} // namespace

int run_material_command(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 2;
  }
  if (std::string_view{argv[1]} == "inspect")
    return inspect_package(argc, argv);
  if (std::string_view{argv[1]} == "build")
    return build_package(argc, argv);
  print_usage();
  return 2;
}
