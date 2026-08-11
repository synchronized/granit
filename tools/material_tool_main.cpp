// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_debug_json.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) {
    return {};
  }
  const auto size = stream.tellg();
  if (size <= 0 ||
      static_cast<std::uint64_t>(size) > granit::material::material_archive_max_file_size) {
    return {};
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), size);
  return stream ? bytes : std::vector<std::byte>{};
}

bool write_file(const std::filesystem::path& path, std::string_view value) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(value.data(), static_cast<std::streamsize>(value.size()));
  return static_cast<bool>(stream);
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 4 && argc != 6) {
    std::cerr << "用法：granit_material_tool inspect <package.grmat> --json "
                 "[--output <debug.json>]\n";
    return 2;
  }
  if (std::string_view{argv[1]} != "inspect" || std::string_view{argv[3]} != "--json" ||
      (argc == 6 && std::string_view{argv[4]} != "--output")) {
    std::cerr << "不支持的参数\n";
    return 2;
  }
  const auto bytes = read_file(argv[2]);
  if (bytes.empty()) {
    std::cerr << "无法读取材质包\n";
    return 1;
  }
  std::string json;
  if (granit::material::export_material_archive_debug_json(bytes, json) !=
      granit::material::archive_error::none) {
    std::cerr << "材质包验证或解码失败\n";
    return 1;
  }
  if (argc == 6) {
    if (!write_file(argv[5], json)) {
      std::cerr << "无法写入调试 JSON\n";
      return 1;
    }
  } else {
    std::cout << json;
  }
  return 0;
}
