// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/arguments.h"
#include "shader_cli/commands.h"
#include "shader_format/shader_cache_key.h"
#include "shader_format/shader_object.h"
#include "shader_object_storage.h"
#include "shader_tools_core.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

namespace {

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return {};
  const auto size = stream.tellg();
  if (size <= 0)
    return {};
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), size);
  return stream ? bytes : std::vector<std::byte>{};
}

std::string read_text(const std::filesystem::path& path) {
  const auto bytes = read_bytes(path);
  if (bytes.empty())
    return {};
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

} // namespace

namespace granit::shader_cli {

int build_shader_fixture_object(int argc, char** argv) {
  const auto spirv_path = option_value(argc, argv, "--spirv");
  const auto wgsl_path = option_value(argc, argv, "--wgsl");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto object_path = option_value(argc, argv, "--output");
  if (!spirv_path || !wgsl_path || !entry || !stage || !object_path ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute")) {
    std::cerr << "fixture-object 需要 --spirv、--wgsl、--entry、--stage 和 --output\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? granit::shader_stage::vertex
                           : *stage == "fragment" ? granit::shader_stage::fragment
                                                  : granit::shader_stage::compute;
  constexpr std::string_view identity = "import-v1";
  constexpr std::string_view target = "portable";
  constexpr std::string_view options = "validated-pair";
  const auto spirv = read_bytes(*spirv_path);
  const auto wgsl = read_text(*wgsl_path);
  granit::tools::shader_info info;
  std::ostringstream output;
  std::ostringstream diagnostic;
  if (spirv.empty() || wgsl.empty() ||
      !granit::tools::inspect_shader(*spirv_path, false, info, output, diagnostic) ||
      info.entry_point != *entry || info.stage != *stage) {
    std::cerr << diagnostic.str() << "无法验证测试 Shader 载荷\n";
    return 1;
  }
  const auto cache_key = granit::detail::shader_format::make_shader_cache_key(
      {wgsl, "wgsl", *entry, *stage, identity, target, options, 0});
  std::vector<std::byte> object;
  if (granit::detail::shader_format::encode_shader_object(
          {wgsl, spirv, granit::tools::serialize_shader_info_json(info), cache_key,
           GRANIT_SHADER_BACKEND_ALL_BITS, 0, stage_value, *entry},
          object) != granit::detail::shader_format::shader_object_error::success) {
    std::cerr << "无法编码测试 Shader Object\n";
    return 1;
  }
  bool cache_hit = false;
  if (granit::tools::store_shader_object(*object_path, object, wgsl, spirv, cache_hit) !=
      granit::detail::shader_format::shader_object_error::success) {
    std::cerr << "无法写入测试 Shader Object\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader Object 未变化：" : "已构建 Shader Object：") << *object_path
            << '\n';
  return 0;
}

} // namespace granit::shader_cli
