// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/arguments.h"
#include "shader_cli/commands.h"
#include <granit/tools/shader_tools.hpp>

#include "shader_format/shader_object.h"
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
  const auto dxc = option_value(argc, argv, "--dxc");
  const auto tint = option_value(argc, argv, "--tint");
  const auto cache = option_value(argc, argv, "--cache");
  const auto output = option_value(argc, argv, "--output");
  const auto index = option_value(argc, argv, "--index");
  if (!manifest || !dxc || !tint || !cache || !output || !index) {
    std::cerr << "build-library 需要 --manifest、--dxc、--tint、--cache、--output 和 --index\n";
    return 2;
  }
  const granit::shader_tools::source_library_desc desc{
      .manifest_path = *manifest,
      .dxc_path = *dxc,
      .tint_path = *tint,
      .cache_path = *cache,
      .output_path = *output,
      .index_path = *index,
  };
  const auto [status, cache_hit] = granit::shader_tools::build_library_from_manifest(desc);
  if (status.failed()) {
    std::cerr << "无法从源清单构建 Shader Library：" << *manifest << '\n';
    return 1;
  }
  std::cout << (cache_hit ? "Shader Library 源构建缓存命中：" : "已构建 Shader Library：")
            << *output << '\n';
  return 0;
}

int link_shader_library(int argc, char** argv) {
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
  std::vector<granit_shader_tools_library_object_desc> objects;
  objects.reserve(object_paths.size());
  for (const auto& path : object_paths) {
    objects.push_back({.struct_size = sizeof(granit_shader_tools_library_object_desc),
                       .reserved = 0,
                       .path = path.data(),
                       .path_length = path.size()});
  }
  const granit_shader_tools_library_desc desc{
      .struct_size = sizeof(granit_shader_tools_library_desc),
      .reserved = 0,
      .objects = objects.data(),
      .object_count = objects.size(),
      .target_backends = backend_mask,
      .reserved2 = 0,
      .output_path = output_path->data(),
      .output_path_length = output_path->size(),
  };
  const auto [status, cache_hit] = granit::shader_tools::build_library(desc);
  if (status.failed()) {
    std::cerr << "无法链接 Shader Library\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader Library 内容未变化：" : "已链接 Shader Library：")
            << *output_path << '\n';
  return 0;
}

int emit_shader_object_ids(int argc, char** argv) {
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
             "// 由 granit_shader_tool object-ids 生成。\n\n"
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
             "// 由 granit_shader_tool index-ids 生成。\n\n"
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
