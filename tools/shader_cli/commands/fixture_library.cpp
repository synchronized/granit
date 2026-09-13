// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/arguments.h"
#include "shader_cli/fixture_commands.h"
#include "shader_format/shader_object.h"
#include "shader_library/builder.h"

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

int link_shader_fixture_library(int argc, char** argv) {
  const auto object_paths = option_values(argc, argv, "--object");
  const auto target = option_value(argc, argv, "--target");
  const auto output_path = option_value(argc, argv, "--output");
  if (object_paths.empty() || !target || !output_path ||
      (*target != "all" && *target != "vulkan" && *target != "webgpu")) {
    std::cerr << "library 需要一个或多个 --object、--target <all|vulkan|webgpu> 和 --output\n";
    return 2;
  }
  const auto backend_mask = *target == "all"      ? GRANIT_SHADER_BACKEND_ALL_BITS
                            : *target == "vulkan" ? GRANIT_SHADER_BACKEND_VULKAN_BIT
                                                  : GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  std::vector<std::filesystem::path> objects;
  objects.reserve(object_paths.size());
  for (const auto& path : object_paths)
    objects.emplace_back(path);
  bool cache_hit = false;
  if (granit::tools::link_shader_library(objects, backend_mask, *output_path, cache_hit) !=
      GRANIT_SUCCESS) {
    std::cerr << "无法链接 Shader Library\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader Library 内容未变化：" : "已链接 Shader Library：")
            << *output_path << '\n';
  return 0;
}

int emit_shader_fixture_object_ids(int argc, char** argv) {
  auto object_specs = option_values(argc, argv, "--object");
  const auto output_path = option_value(argc, argv, "--output");
  if (object_specs.empty() || !output_path) {
    std::cerr << "object-ids 需要一个或多个 --object <name=path> 和 --output\n";
    return 2;
  }
  std::ranges::sort(object_specs);
  std::ostringstream content;
  content << "// SPDX-License-Identifier: MIT\n"
             "// Copyright (c) 2026 Granit contributors\n\n"
             "// 由 Granit 测试夹具工具生成。\n\n"
          << std::hex << std::setfill('0');
  std::string previous_name;
  for (const auto& spec : object_specs) {
    const auto separator = spec.find('=');
    const auto name = spec.substr(0, separator);
    const auto valid_name =
        separator != std::string::npos && separator + 1 < spec.size() && valid_identifier(name);
    if (!valid_name || name == previous_name) {
      std::cerr << "object-ids 的名称必须是唯一 C++ 标识符：" << spec << '\n';
      return 2;
    }
    const auto object_path = spec.substr(separator + 1);
    const auto manifest = read_bytes(object_path);
    granit::detail::shader_format::shader_object_view object;
    if (manifest.empty() || granit::detail::shader_format::decode_shader_object(manifest, object) !=
                                granit::detail::shader_format::shader_object_error::success) {
      std::cerr << "无法读取 Shader Object清单：" << object_path << '\n';
      return 1;
    }
    append_content_id(content, name, object.content_id);
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
