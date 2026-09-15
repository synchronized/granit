// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_formats/shader/shader_cache_key.h"
#include "asset_formats/shader/shader_object.h"
#include "asset_tools/shader/compiler_internal.h"
#include "asset_tools/shader/object_storage.h"
#include "granit_asset_tool/shader/arguments.h"
#include "shader_fixture_tool/commands.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
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

namespace granit::asset_tools::cli {

int build_shader_fixture_object(int argc, char** argv) {
  const auto spirv_path = option_value(argc, argv, "--spirv");
  const auto wgsl_path = option_value(argc, argv, "--wgsl");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto object_path = option_value(argc, argv, "--output");
  const auto source_path = option_value(argc, argv, "--source");
  const auto dxc_path = option_value(argc, argv, "--dxc");
  const auto tint_path = option_value(argc, argv, "--tint");
  if (!spirv_path || !wgsl_path || !entry || !stage || !object_path ||
      (source_path && (!dxc_path || !tint_path)) ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute")) {
    std::cerr << "object 需要 --spirv、--wgsl、--entry、--stage 和 --output\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? granit::shader_stage::vertex
                           : *stage == "fragment" ? granit::shader_stage::fragment
                                                  : granit::shader_stage::compute;
  constexpr std::string_view target = "portable";
  const auto spirv = read_bytes(*spirv_path);
  const auto wgsl = read_text(*wgsl_path);
  granit::asset_tools::detail::shader_info info;
  std::ostringstream output;
  std::ostringstream diagnostic;
  if (spirv.empty() || wgsl.empty() ||
      !granit::asset_tools::detail::inspect_shader(*spirv_path, false, info, output, diagnostic) ||
      info.entry_point != *entry || info.stage != *stage) {
    std::cerr << diagnostic.str() << "无法验证测试 Shader 载荷\n";
    return 1;
  }
  std::string source = wgsl;
  std::string source_kind = "wgsl";
  std::string identity = "import-v1";
  std::string options = "validated-pair";
  if (source_path) {
    source = read_text(*source_path);
    const auto dxc_identity = granit::asset_tools::detail::file_sha256_hex(*dxc_path);
    const auto tint_identity = granit::asset_tools::detail::file_sha256_hex(*tint_path);
    if (source.empty() || dxc_identity.empty() || tint_identity.empty()) {
      std::cerr << "无法读取测试 HLSL 或工具身份\n";
      return 1;
    }
    source_kind = "hlsl";
    identity = "dxc=" + dxc_identity + ";tint=" + tint_identity;
    options = "source=hlsl;spirv=vulkan1.3;bridge=spirv1.3";
    auto definitions = option_values(argc, argv, "--define");
    std::ranges::sort(definitions);
    for (const auto& definition : definitions) {
      const auto separator = definition.find('=');
      if (separator == std::string::npos || separator == 0 || separator + 1 == definition.size()) {
        std::cerr << "测试 Shader Define 无效\n";
        return 1;
      }
      const auto name = std::string_view{definition}.substr(0, separator);
      const auto value = std::string_view{definition}.substr(separator + 1);
      options += ";define=" + std::to_string(name.size()) + ":" + std::string{name} + ":" +
                 std::to_string(value.size()) + ":" + std::string{value};
    }
  }
  constexpr std::string_view hlsl_target = "vulkan1.3+webgpu-portable";
  const auto cache_key = granit::detail::shader_format::make_shader_cache_key(
      {source, source_kind, *entry, *stage, identity, source_path ? hlsl_target : target, options,
       0});
  std::vector<std::byte> object;
  if (granit::detail::shader_format::encode_shader_object(
          {wgsl, spirv, granit::asset_tools::detail::serialize_shader_info_json(info), cache_key,
           GRANIT_SHADER_BACKEND_ALL_BITS, 0, stage_value, *entry},
          object) != granit::detail::shader_format::shader_object_error::success) {
    std::cerr << "无法编码测试 Shader Object\n";
    return 1;
  }
  bool cache_hit = false;
  if (granit::asset_tools::detail::store_shader_object(*object_path, object, wgsl, spirv,
                                                       cache_hit) !=
      granit::detail::shader_format::shader_object_error::success) {
    std::cerr << "无法写入测试 Shader Object\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader Object 未变化：" : "已构建 Shader Object：") << *object_path
            << '\n';
  return 0;
}

} // namespace granit::asset_tools::cli
