// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/arguments.h"
#include "shader_cli/commands.h"
#include <granit/tools/asset_tools.hpp>

#include "shader_library/source_manifest.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

namespace granit::shader_cli {
namespace {
std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> output(bytes.size());
  if (!bytes.empty())
    std::memcpy(output.data(), bytes.data(), bytes.size());
  return output;
}

std::string read_text(const std::filesystem::path& path) {
  const auto bytes = read_bytes(path);
  if (bytes.empty())
    return {};
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

bool valid_identifier(std::string_view name) {
  return !name.empty() &&
         ((name.front() >= 'a' && name.front() <= 'z') ||
          (name.front() >= 'A' && name.front() <= 'Z') || name.front() == '_') &&
         std::ranges::all_of(name, [](const unsigned char value) {
           return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                  (value >= '0' && value <= '9') || value == '_';
         });
}

void append_content_id(std::ostringstream& content, std::string_view name,
                       const granit::shader_content_id& content_id) {
  content << "constexpr std::array<std::byte, 32> " << name << "{\n";
  for (const auto value : content_id)
    content << "  std::byte{0x" << std::setw(2) << std::to_integer<unsigned int>(value) << "},\n";
  content << "};\n";
}

bool write_if_changed(const std::filesystem::path& destination, std::string_view content) {
  std::error_code error;
  if (!destination.parent_path().empty()) {
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error)
      return false;
  }
  if (std::filesystem::exists(destination, error) && !error) {
    const auto current = read_bytes(destination);
    if (current.size() == content.size() &&
        std::memcmp(current.data(), content.data(), content.size()) == 0)
      return true;
  }
  std::ofstream stream{destination, std::ios::binary | std::ios::trunc};
  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
  return static_cast<bool>(stream);
}

} // namespace

int build_shader_library(int argc, char** argv) {
  const auto manifest = option_value(argc, argv, "--manifest");
  const auto toolchain = option_value(argc, argv, "--toolchain");
  const auto cache = option_value(argc, argv, "--cache");
  const auto output = option_value(argc, argv, "--output");
  const auto index = option_value(argc, argv, "--index");
  if (!manifest || !toolchain || !cache || !output || !index) {
    std::cerr << "build-library 需要 --manifest、--toolchain、--cache、--output 和 --index\n";
    return 2;
  }
  const granit::asset_tools::shader::source_library_desc desc{
      .manifest_path = *manifest,
      .toolchain_root = *toolchain,
      .cache_path = *cache,
      .output_path = *output,
      .index_path = *index,
  };
  const auto [status, cache_hit] = granit::asset_tools::shader::build_library_from_manifest(desc);
  if (status.failed()) {
    std::cerr << "无法从源清单构建 Shader Library：" << *manifest << '\n';
    return 1;
  }
  std::cout << (cache_hit ? "Shader Library 源构建缓存命中：" : "已构建 Shader Library：")
            << *output << '\n';
  return 0;
}

int emit_shader_index_ids(int argc, char** argv) {
  const auto index_path = option_value(argc, argv, "--index");
  auto shader_specs = option_values(argc, argv, "--shader");
  const auto output_path = option_value(argc, argv, "--output");
  if (!index_path || shader_specs.empty() || !output_path) {
    std::cerr << "index-ids 需要 --index、一个或多个 --shader <name=logical-name> 和 --output\n";
    return 2;
  }
  granit::tools::shader_library_index index;
  if (granit::tools::parse_shader_library_index_json(read_text(*index_path), index) !=
      granit::tools::shader_library_source_error::none) {
    std::cerr << "无法读取 Shader Library 索引：" << *index_path << '\n';
    return 1;
  }
  std::ranges::sort(shader_specs);
  std::ostringstream content;
  content << "// SPDX-License-Identifier: MIT\n"
             "// Copyright (c) 2026 Granit contributors\n\n"
             "// 由 granit_asset_tool shader index-ids 生成。\n\n"
          << std::hex << std::setfill('0');
  std::string previous_name;
  for (const auto& spec : shader_specs) {
    const auto separator = spec.find('=');
    const auto name = spec.substr(0, separator);
    const std::string_view spec_view{spec};
    const auto logical_name =
        separator == std::string::npos ? std::string_view{} : spec_view.substr(separator + 1);
    if (!valid_identifier(name) || logical_name.empty() || name == previous_name) {
      std::cerr << "index-ids 的名称必须是唯一 C++ 标识符：" << spec << '\n';
      return 2;
    }
    const auto found = std::ranges::find(index.shaders, logical_name,
                                         &granit::tools::shader_library_index_entry::name);
    if (found == index.shaders.end()) {
      std::cerr << "索引中不存在 Shader 逻辑名称：" << logical_name << '\n';
      return 1;
    }
    append_content_id(content, name, found->content_id);
    previous_name = name;
  }
  const auto generated = content.str();
  if (!write_if_changed(std::filesystem::path{*output_path}, generated)) {
    std::cerr << "无法写入内容 ID：" << *output_path << '\n';
    return 1;
  }
  return 0;
}

} // namespace granit::shader_cli
