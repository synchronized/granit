// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_debug_json.h"
#include "material/material_package_archive.h"
#include "material/material_source_json.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

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

std::string read_text_file(const std::filesystem::path& path) {
  const auto bytes = read_file(path);
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::filesystem::path temporary_path(const std::filesystem::path& path) {
#if defined(_WIN32)
  const auto process_id = static_cast<unsigned long>(GetCurrentProcessId());
#else
  const auto process_id = static_cast<unsigned long>(getpid());
#endif
  return std::filesystem::path{path.string() + ".tmp." + std::to_string(process_id)};
}

bool replace_file(const std::filesystem::path& temporary, const std::filesystem::path& target) {
#if defined(_WIN32)
  return MoveFileExW(temporary.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code error;
  std::filesystem::rename(temporary, target, error);
  return !error;
#endif
}

bool write_file_atomic(const std::filesystem::path& path, std::span<const std::byte> value) {
  const auto temporary = temporary_path(path);
  std::error_code ignored;
  std::filesystem::remove(temporary, ignored);
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(value.data()),
                 static_cast<std::streamsize>(value.size()));
    stream.flush();
    if (!stream) {
      std::filesystem::remove(temporary, ignored);
      return false;
    }
  }
  if (!replace_file(temporary, path)) {
    std::filesystem::remove(temporary, ignored);
    return false;
  }
  return true;
}

bool write_file_atomic(const std::filesystem::path& path, std::string_view value) {
  return write_file_atomic(path, std::as_bytes(std::span{value}));
}

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_material_tool build <source.grmat.json> --output <package.grmat> "
               "[--emit-debug-json [debug.json]]\n"
               "  granit_material_tool inspect <package.grmat> --json "
               "[--output <debug.json>]\n";
}

int inspect_package(int argc, char** argv) {
  if ((argc != 4 && argc != 6) || std::string_view{argv[3]} != "--json" ||
      (argc == 6 && std::string_view{argv[4]} != "--output")) {
    print_usage();
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
    if (!write_file_atomic(argv[5], json)) {
      std::cerr << "无法原子写入调试 JSON\n";
      return 1;
    }
  } else {
    std::cout << json;
  }
  return 0;
}

int build_package(int argc, char** argv) {
  if (argc < 5 || argc > 7 || std::string_view{argv[3]} != "--output") {
    print_usage();
    return 2;
  }
  bool emit_debug_json = false;
  std::filesystem::path debug_path;
  if (argc >= 6) {
    if (std::string_view{argv[5]} != "--emit-debug-json") {
      print_usage();
      return 2;
    }
    emit_debug_json = true;
    if (argc == 7) {
      debug_path = argv[6];
    }
  }
  const std::filesystem::path source_path = argv[2];
  const std::filesystem::path output_path = argv[4];
  const auto source = read_text_file(source_path);
  if (source.empty()) {
    std::cerr << "无法读取材质源描述\n";
    return 1;
  }
  granit::material::material_package package;
  if (granit::material::parse_material_source_json(source, source_path.parent_path(), package) !=
      granit::material::source_json_error::none) {
    std::cerr << "材质源描述、引用文件或包语义无效\n";
    return 1;
  }
  std::vector<std::byte> bytes;
  if (granit::material::encode_material_package_archive(package, bytes) !=
          granit::material::archive_error::none ||
      !write_file_atomic(output_path, bytes)) {
    std::cerr << "无法编码或原子写入材质包\n";
    return 1;
  }
  if (emit_debug_json) {
    std::string json;
    if (debug_path.empty()) {
      debug_path = std::filesystem::path{output_path.string() + ".debug.json"};
    }
    if (granit::material::export_material_archive_debug_json(bytes, json) !=
            granit::material::archive_error::none ||
        !write_file_atomic(debug_path, json)) {
      std::cerr << "材质包已生成，但无法导出调试 JSON\n";
      return 1;
    }
  }
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 2;
  }
  if (std::string_view{argv[1]} == "inspect") {
    return inspect_package(argc, argv);
  }
  if (std::string_view{argv[1]} == "build") {
    return build_package(argc, argv);
  }
  std::cerr << "不支持的命令\n";
  print_usage();
  return 2;
}
