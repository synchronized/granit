// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/sha256.h"
#include "shader_asset.h"
#include "shader_format/shader_library.h"
#include <granit/tools/shader_tools.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::optional<std::string> option_value(int argc, char** argv, std::string_view name);

std::vector<std::string> option_values(int argc, char** argv, std::string_view name) {
  std::vector<std::string> values;
  for (int index = 2; index + 1 < argc; ++index) {
    if (argv[index] == name)
      values.emplace_back(argv[index + 1]);
  }
  return values;
}

bool parse_defines(const std::vector<std::string>& arguments,
                   std::vector<std::pair<std::string, std::string>>& output) {
  output.clear();
  output.reserve(arguments.size());
  for (const auto& argument : arguments) {
    const auto separator = argument.find('=');
    if (separator == std::string::npos || separator == 0 || separator + 1 == argument.size())
      return false;
    output.emplace_back(argument.substr(0, separator), argument.substr(separator + 1));
  }
  std::ranges::sort(output);
  return std::ranges::adjacent_find(output, [](const auto& left, const auto& right) {
           return left.first == right.first;
         }) == output.end();
}

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> output(bytes.size());
  if (!bytes.empty())
    std::memcpy(output.data(), bytes.data(), bytes.size());
  return output;
}

int link_shader_library(int argc, char** argv) {
  const auto asset_paths = option_values(argc, argv, "--asset");
  const auto target = option_value(argc, argv, "--target");
  const auto output_path = option_value(argc, argv, "--output");
  if (asset_paths.empty() || !target || !output_path ||
      (*target != "all" && *target != "vulkan" && *target != "webgpu")) {
    std::cerr << "library 需要一个或多个 --asset、--target <all|vulkan|webgpu> 和 --output\n";
    return 2;
  }
  const auto backend_mask = *target == "all"      ? GRANIT_SHADER_BACKEND_ALL_BITS
                            : *target == "vulkan" ? GRANIT_SHADER_BACKEND_VULKAN_BIT
                                                  : GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  struct owned_asset {
    std::vector<std::byte> manifest;
    std::vector<std::byte> wgsl;
    std::vector<std::byte> spirv;
  };
  std::vector<owned_asset> owned;
  owned.reserve(asset_paths.size());
  for (const auto& asset_path : asset_paths) {
    owned_asset asset{.manifest = read_bytes(asset_path), .wgsl = {}, .spirv = {}};
    if ((backend_mask & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0)
      asset.wgsl = read_bytes(asset_path + ".wgsl");
    if ((backend_mask & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0)
      asset.spirv = read_bytes(asset_path + ".spv");
    if (asset.manifest.empty() ||
        ((backend_mask & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0 && asset.wgsl.empty()) ||
        ((backend_mask & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0 && asset.spirv.empty())) {
      std::cerr << "无法读取 Shader 资产或目标 sidecar：" << asset_path << '\n';
      return 1;
    }
    owned.push_back(std::move(asset));
  }
  std::vector<granit::detail::shader_format::shader_library_asset_source> sources;
  sources.reserve(owned.size());
  for (const auto& asset : owned)
    sources.push_back({asset.manifest, asset.wgsl, asset.spirv});
  std::vector<std::byte> library;
  if (granit::detail::shader_format::encode_shader_library({sources, backend_mask}, library) !=
      granit::detail::shader_format::shader_library_error::success) {
    std::cerr << "无法链接 Shader Library\n";
    return 1;
  }
  const std::filesystem::path destination{*output_path};
  std::error_code error;
  if (std::filesystem::exists(destination, error) && !error && read_bytes(destination) == library) {
    std::cout << "Shader Library 内容未变化：" << destination << '\n';
    return 0;
  }
  if (!destination.parent_path().empty()) {
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) {
      std::cerr << "无法创建 Shader Library 输出目录：" << error.message() << '\n';
      return 1;
    }
  }
  auto temporary = destination;
  temporary += ".tmp";
  {
    std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
    stream.write(reinterpret_cast<const char*>(library.data()),
                 static_cast<std::streamsize>(library.size()));
    if (!stream) {
      std::cerr << "无法写入 Shader Library：" << temporary << '\n';
      return 1;
    }
  }
  std::filesystem::remove(destination, error);
  error.clear();
  std::filesystem::rename(temporary, destination, error);
  if (error) {
    std::filesystem::remove(temporary);
    std::cerr << "无法提交 Shader Library：" << error.message() << '\n';
    return 1;
  }
  std::cout << "已链接 Shader Library：" << destination << '\n';
  return 0;
}

int pack_shader_asset(int argc, char** argv) {
  const auto spirv_path = option_value(argc, argv, "--spirv");
  const auto wgsl_path = option_value(argc, argv, "--wgsl");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto asset_path = option_value(argc, argv, "--asset");
  if (!spirv_path || !wgsl_path || !entry || !stage || !asset_path ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute")) {
    std::cerr << "pack 需要 --spirv、--wgsl、--entry、--stage 和 --asset\n";
    return 2;
  }
  const auto spirv = read_bytes(*spirv_path);
  const auto wgsl_bytes = read_bytes(*wgsl_path);
  if (spirv.empty() || wgsl_bytes.empty()) {
    std::cerr << "无法读取 Shader 输入\n";
    return 1;
  }
  granit_shader_tools_inspect_desc inspect_desc{};
  inspect_desc.struct_size = sizeof(inspect_desc);
  inspect_desc.input_path = spirv_path->data();
  inspect_desc.input_path_length = spirv_path->size();
  auto [inspect_status, inspect_result] = granit::shader_tools::inspect_spirv(inspect_desc);
  const auto inspected = inspect_result.info();
  const auto stage_value = *stage == "vertex"     ? granit::shader_stage::vertex
                           : *stage == "fragment" ? granit::shader_stage::fragment
                                                  : granit::shader_stage::compute;
  if (inspect_status.failed() || inspected.stage != stage_value ||
      inspected.entry_point != *entry) {
    std::cerr << "SPIR-V 的阶段或入口与 pack 参数不一致\n" << inspected.diagnostic;
    return 1;
  }
  const std::string reflection_json{inspect_result.reflection_json()};
  if (reflection_json.empty()) {
    std::cerr << "无法提取 SPIR-V 反射信息\n";
    return 1;
  }
  const std::string_view wgsl{reinterpret_cast<const char*>(wgsl_bytes.data()), wgsl_bytes.size()};
  granit::detail::shader_format::shader_asset_source source{.wgsl = wgsl,
                                                            .spirv = spirv,
                                                            .reflection_json = reflection_json,
                                                            .cache_key =
                                                                granit::detail::sha256_bytes(spirv),
                                                            .backend_mask = 3,
                                                            .required_features = 0,
                                                            .stage = stage_value,
                                                            .entry_point = *entry};
  std::vector<std::byte> manifest;
  if (granit::detail::shader_format::encode_shader_asset(source, manifest) !=
      granit::detail::shader_format::shader_asset_error::success) {
    std::cerr << "无法编码 Shader Asset\n";
    return 1;
  }
  bool cache_hit = false;
  if (granit::tools::store_shader_asset(*asset_path, manifest, wgsl, spirv, cache_hit) !=
      granit::detail::shader_format::shader_asset_error::success) {
    std::cerr << "无法写入 Shader Asset\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader 资产未变化：" : "已打包 Shader 资产：") << *asset_path << '\n';
  return 0;
}

int emit_shader_asset_ids(int argc, char** argv) {
  auto asset_specs = option_values(argc, argv, "--asset");
  const auto output_path = option_value(argc, argv, "--output");
  if (asset_specs.empty() || !output_path) {
    std::cerr << "asset-ids 需要一个或多个 --asset <name=path> 和 --output\n";
    return 2;
  }
  std::ranges::sort(asset_specs);
  std::ostringstream content;
  content << "// SPDX-License-Identifier: MIT\n"
             "// Copyright (c) 2026 Granit contributors\n\n"
             "// 由 granit_shader_tool asset-ids 生成。\n\n"
          << std::hex << std::setfill('0');
  std::string previous_name;
  for (const auto& spec : asset_specs) {
    const auto separator = spec.find('=');
    const auto name = spec.substr(0, separator);
    const auto valid_name =
        separator != std::string::npos && separator != 0 && separator + 1 < spec.size() &&
        ((name.front() >= 'a' && name.front() <= 'z') ||
         (name.front() >= 'A' && name.front() <= 'Z') || name.front() == '_') &&
        std::ranges::all_of(name, [](const unsigned char value) {
          return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                 (value >= '0' && value <= '9') || value == '_';
        });
    if (!valid_name || name == previous_name) {
      std::cerr << "asset-ids 的名称必须是唯一 C++ 标识符：" << spec << '\n';
      return 2;
    }
    const auto asset_path = spec.substr(separator + 1);
    const auto manifest = read_bytes(asset_path);
    granit::detail::shader_format::shader_asset_view asset;
    if (manifest.empty() || granit::detail::shader_format::decode_shader_asset(manifest, asset) !=
                                granit::detail::shader_format::shader_asset_error::success) {
      std::cerr << "无法读取 Shader 资产清单：" << asset_path << '\n';
      return 1;
    }
    content << "constexpr std::array<std::byte, 32> " << name << "{\n";
    for (const auto value : asset.content_id)
      content << "  std::byte{0x" << std::setw(2) << std::to_integer<unsigned int>(value) << "},\n";
    content << "};\n";
    previous_name = name;
  }
  const std::filesystem::path destination{*output_path};
  std::error_code error;
  if (!destination.parent_path().empty()) {
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) {
      std::cerr << "无法创建内容 ID 输出目录：" << error.message() << '\n';
      return 1;
    }
  }
  const auto generated = content.str();
  if (std::filesystem::exists(destination, error) && !error) {
    const auto current = read_bytes(destination);
    if (current.size() == generated.size() &&
        std::memcmp(current.data(), generated.data(), generated.size()) == 0)
      return 0;
  }
  std::ofstream stream{destination, std::ios::binary | std::ios::trunc};
  stream.write(generated.data(), static_cast<std::streamsize>(generated.size()));
  if (!stream) {
    std::cerr << "无法写入内容 ID：" << destination << '\n';
    return 1;
  }
  return 0;
}

std::optional<std::string> option_value(int argc, char** argv, std::string_view name) {
  for (int index = 2; index + 1 < argc; ++index) {
    if (argv[index] == name)
      return argv[index + 1];
  }
  return std::nullopt;
}

std::optional<std::string> resolve_tool_identity(std::string_view path,
                                                 const std::optional<std::string>& supplied) {
  if (supplied)
    return supplied;
  auto [status, digest] = granit::shader_tools::tool_identity(path);
  if (status.failed())
    return std::nullopt;
  return "sha256=" + digest;
}

int compile_shader(int argc, char** argv) {
  const auto tint = option_value(argc, argv, "--tint");
  const auto input = option_value(argc, argv, "--input");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto output = option_value(argc, argv, "--output");
  const auto asset = option_value(argc, argv, "--asset");
  const auto tint_revision = option_value(argc, argv, "--tint-revision");
  const auto target_environment = option_value(argc, argv, "--target-environment");
  const auto asset_backend = option_value(argc, argv, "--asset-backend");
  const auto features = option_value(argc, argv, "--features");
  if (!tint || !input || !entry || !stage || !output ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute") ||
      (asset_backend && !asset) || (features && !asset) ||
      (asset_backend && *asset_backend != "all" && *asset_backend != "vulkan" &&
       *asset_backend != "webgpu") ||
      (features && *features != "none" && *features != "float16" && *features != "subgroup")) {
    std::cerr << "compile 需要 --tint、--input、--entry、--stage 和 --output\n";
    std::cerr << "可选 --tint-revision、--target-environment、"
                 "--asset-backend <all|vulkan|webgpu> 和 --features <none|float16|subgroup>\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? GRANIT_SHADER_STAGE_VERTEX
                           : *stage == "fragment" ? GRANIT_SHADER_STAGE_FRAGMENT
                                                  : GRANIT_SHADER_STAGE_COMPUTE;
  constexpr std::string_view default_target = "vulkan1.3";
  constexpr std::string_view compile_options = "format=spirv;validate=1";
  const auto target = target_environment ? std::string_view{*target_environment} : default_target;
  const auto backend_mask = !asset_backend || *asset_backend == "all"
                                ? GRANIT_SHADER_BACKEND_ALL_BITS
                            : *asset_backend == "vulkan" ? GRANIT_SHADER_BACKEND_VULKAN_BIT
                                                         : GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  const auto required_features = !features || *features == "none" ? UINT64_C(0)
                                 : *features == "float16" ? GRANIT_SHADER_FEATURE_FLOAT16_BIT
                                                          : GRANIT_SHADER_FEATURE_SUBGROUP_BIT;
  if (required_features != 0) {
    std::cerr << "目标 portable 档位不支持必需特性：" << *features << '\n';
    return 1;
  }
  const auto tint_identity =
      asset ? resolve_tool_identity(*tint, tint_revision) : std::optional<std::string>{""};
  if (!tint_identity) {
    std::cerr << "无法读取 Tint 工具身份\n";
    return 1;
  }
  if (asset) {
    granit_shader_tools_cache_desc cache{};
    cache.struct_size = sizeof(cache);
    cache.source_path = input->data();
    cache.source_path_length = input->size();
    cache.source_language = GRANIT_SHADER_SOURCE_LANGUAGE_WGSL;
    cache.spirv_output_path = output->data();
    cache.spirv_output_path_length = output->size();
    cache.asset_path = asset->data();
    cache.asset_path_length = asset->size();
    cache.entry_point = entry->data();
    cache.entry_point_length = entry->size();
    cache.stage = stage_value;
    cache.tint_revision = tint_identity->data();
    cache.tint_revision_length = tint_identity->size();
    cache.target_environment = target.data();
    cache.target_environment_length = target.size();
    cache.compile_options = compile_options.data();
    cache.compile_options_length = compile_options.size();
    cache.backend_mask = backend_mask;
    cache.required_features = required_features;
    const auto [cache_status, cache_hit] = granit::shader_tools::restore_asset_cache(cache);
    if (cache_status.failed()) {
      std::cerr << "Shader 资产缓存查询失败\n";
      return 1;
    }
    if (cache_hit) {
      std::cout << "Shader 资产缓存命中：" << *asset << '\n';
      return 0;
    }
  }
  granit::shader_tools::compiler compiler;
  if (compiler.initialize({{}, *tint}).failed()) {
    std::cerr << "Shader Compiler 创建失败\n";
    return 1;
  }
  granit::shader_tools::compile_desc desc;
  desc.input_path = *input;
  desc.source_language = granit::shader_source_language::wgsl;
  desc.stage = static_cast<granit::shader_stage>(stage_value);
  desc.entry_point = *entry;
  desc.target_backends = static_cast<granit::shader_backend>(backend_mask);
  desc.spirv_output_path = *output;
  auto [status, result] = compiler.compile(desc);
  const auto info = result.info();
  std::cout << info.output;
  std::cerr << info.diagnostic;
  if (status.ok() && asset) {
    granit_shader_tools_asset_desc asset_desc{};
    asset_desc.struct_size = sizeof(asset_desc);
    asset_desc.source_path = input->data();
    asset_desc.source_path_length = input->size();
    asset_desc.source_language = GRANIT_SHADER_SOURCE_LANGUAGE_WGSL;
    asset_desc.wgsl_path = input->data();
    asset_desc.wgsl_path_length = input->size();
    asset_desc.spirv_path = output->data();
    asset_desc.spirv_path_length = output->size();
    asset_desc.output_path = asset->data();
    asset_desc.output_path_length = asset->size();
    asset_desc.tint_revision = tint_identity->data();
    asset_desc.tint_revision_length = tint_identity->size();
    asset_desc.target_environment = target.data();
    asset_desc.target_environment_length = target.size();
    asset_desc.compile_options = compile_options.data();
    asset_desc.compile_options_length = compile_options.size();
    asset_desc.backend_mask = backend_mask;
    asset_desc.required_features = required_features;
    const auto [asset_status, cache_hit] = result.write_asset(asset_desc);
    if (asset_status.failed()) {
      std::cerr << "Shader 资产写入失败\n";
      return 1;
    }
    std::cout << (cache_hit ? "Shader 资产缓存命中：" : "已生成 Shader 资产：") << *asset << '\n';
  }
  return status.ok() ? 0 : 1;
}

int compile_hlsl_shader(int argc, char** argv) {
  const auto dxc = option_value(argc, argv, "--dxc");
  const auto tint = option_value(argc, argv, "--tint");
  const auto input = option_value(argc, argv, "--input");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto spirv_output = option_value(argc, argv, "--spirv-output");
  const auto wgsl_output = option_value(argc, argv, "--wgsl-output");
  const auto asset = option_value(argc, argv, "--asset");
  const auto dxc_revision = option_value(argc, argv, "--dxc-revision");
  const auto tint_revision = option_value(argc, argv, "--tint-revision");
  const auto asset_backend = option_value(argc, argv, "--asset-backend");
  std::vector<std::pair<std::string, std::string>> definitions;
  const auto define_arguments = option_values(argc, argv, "--define");
  if (!dxc || !tint || !input || !entry || !stage || !spirv_output || !wgsl_output ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute") ||
      (asset_backend && !asset) ||
      (asset_backend && *asset_backend != "all" && *asset_backend != "vulkan" &&
       *asset_backend != "webgpu") ||
      !parse_defines(define_arguments, definitions)) {
    std::cerr << "compile-hlsl 需要 --dxc、--tint、--input、--entry、--stage、"
                 "--spirv-output 和 --wgsl-output\n";
    std::cerr << "可选 --dxc-revision 与 --tint-revision 可覆盖自动二进制身份\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? GRANIT_SHADER_STAGE_VERTEX
                           : *stage == "fragment" ? GRANIT_SHADER_STAGE_FRAGMENT
                                                  : GRANIT_SHADER_STAGE_COMPUTE;
  const auto backend_mask = !asset_backend || *asset_backend == "all"
                                ? GRANIT_SHADER_BACKEND_ALL_BITS
                            : *asset_backend == "vulkan" ? GRANIT_SHADER_BACKEND_VULKAN_BIT
                                                         : GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  const auto dxc_identity =
      asset ? resolve_tool_identity(*dxc, dxc_revision) : std::optional<std::string>{""};
  const auto tint_identity =
      asset ? resolve_tool_identity(*tint, tint_revision) : std::optional<std::string>{""};
  if (!dxc_identity || !tint_identity) {
    std::cerr << "无法读取 DXC 或 Tint 工具身份\n";
    return 1;
  }
  const std::string revisions = "dxc=" + *dxc_identity + ";tint=" + *tint_identity;
  constexpr std::string_view target = "vulkan1.3+webgpu-portable";
  std::string options = "source=hlsl;spirv=vulkan1.3;bridge=spirv1.3";
  for (const auto& [name, value] : definitions) {
    options += ";define=" + std::to_string(name.size()) + ":" + name + ":" +
               std::to_string(value.size()) + ":" + value;
  }
  if (asset && backend_mask == GRANIT_SHADER_BACKEND_ALL_BITS) {
    granit_shader_tools_cache_desc cache{};
    cache.struct_size = sizeof(cache);
    cache.source_path = input->data();
    cache.source_path_length = input->size();
    cache.source_language = GRANIT_SHADER_SOURCE_LANGUAGE_HLSL;
    cache.wgsl_output_path = wgsl_output->data();
    cache.wgsl_output_path_length = wgsl_output->size();
    cache.spirv_output_path = spirv_output->data();
    cache.spirv_output_path_length = spirv_output->size();
    cache.asset_path = asset->data();
    cache.asset_path_length = asset->size();
    cache.entry_point = entry->data();
    cache.entry_point_length = entry->size();
    cache.stage = stage_value;
    cache.tint_revision = revisions.data();
    cache.tint_revision_length = revisions.size();
    cache.target_environment = target.data();
    cache.target_environment_length = target.size();
    cache.compile_options = options.data();
    cache.compile_options_length = options.size();
    cache.backend_mask = backend_mask;
    const auto [cache_status, cache_hit] = granit::shader_tools::restore_asset_cache(cache);
    if (cache_status.failed()) {
      std::cerr << "HLSL Shader 资产缓存查询失败\n";
      return 1;
    }
    if (cache_hit) {
      std::cout << "Shader 资产缓存命中：" << *asset << '\n';
      return 0;
    }
  }

  std::vector<granit::shader_tools::shader_define> shader_definitions;
  shader_definitions.reserve(definitions.size());
  for (const auto& [name, value] : definitions) {
    shader_definitions.push_back({.name = name, .value = value});
  }
  granit::shader_tools::compiler compiler;
  if (compiler.initialize({*dxc, *tint}).failed()) {
    std::cerr << "Shader Compiler 创建失败\n";
    return 1;
  }
  granit::shader_tools::compile_desc desc;
  desc.input_path = *input;
  desc.source_language = granit::shader_source_language::hlsl;
  desc.stage = static_cast<granit::shader_stage>(stage_value);
  desc.entry_point = *entry;
  desc.target_backends = static_cast<granit::shader_backend>(backend_mask);
  desc.spirv_output_path = *spirv_output;
  desc.wgsl_output_path = *wgsl_output;
  desc.defines = shader_definitions;
  auto [status, result] = compiler.compile(desc);
  const auto info = result.info();
  std::cout << info.output;
  std::cerr << info.diagnostic;
  if (status.failed() || !asset)
    return status.ok() ? 0 : 1;

  granit_shader_tools_asset_desc asset_desc{};
  asset_desc.struct_size = sizeof(asset_desc);
  asset_desc.source_path = input->data();
  asset_desc.source_path_length = input->size();
  asset_desc.source_language = GRANIT_SHADER_SOURCE_LANGUAGE_HLSL;
  asset_desc.wgsl_path = wgsl_output->data();
  asset_desc.wgsl_path_length = wgsl_output->size();
  asset_desc.spirv_path = spirv_output->data();
  asset_desc.spirv_path_length = spirv_output->size();
  asset_desc.output_path = asset->data();
  asset_desc.output_path_length = asset->size();
  asset_desc.tint_revision = revisions.data();
  asset_desc.tint_revision_length = revisions.size();
  asset_desc.target_environment = target.data();
  asset_desc.target_environment_length = target.size();
  asset_desc.compile_options = options.data();
  asset_desc.compile_options_length = options.size();
  asset_desc.backend_mask = backend_mask;
  const auto [asset_status, cache_hit] = result.write_asset(asset_desc);
  if (asset_status.failed()) {
    std::cerr << "HLSL Shader 资产写入失败\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader 资产内容未变化：" : "已生成 Shader 资产：") << *asset << '\n';
  return 0;
}

std::string json_string(std::string_view value) {
  std::ostringstream output;
  output << '"';
  for (const auto character : value) {
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20) {
        constexpr char hex[] = "0123456789abcdef";
        output << "\\u00" << hex[(static_cast<unsigned char>(character) >> 4) & 0x0f]
               << hex[static_cast<unsigned char>(character) & 0x0f];
      } else {
        output << character;
      }
      break;
    }
  }
  output << '"';
  return std::move(output).str();
}

const char* binding_type_name(uint32_t type) {
  switch (type) {
  case GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER:
    return "uniform_buffer";
  case GRANIT_SHADER_TOOLS_BINDING_STORAGE_BUFFER:
    return "storage_buffer";
  case GRANIT_SHADER_TOOLS_BINDING_SAMPLED_TEXTURE:
    return "sampled_texture";
  case GRANIT_SHADER_TOOLS_BINDING_STORAGE_TEXTURE:
    return "storage_texture";
  case GRANIT_SHADER_TOOLS_BINDING_SAMPLER:
    return "sampler";
  default:
    return "unsupported";
  }
}

const char* binding_access_name(uint32_t access) {
  switch (access) {
  case GRANIT_SHADER_TOOLS_ACCESS_READ:
    return "read";
  case GRANIT_SHADER_TOOLS_ACCESS_WRITE:
    return "write";
  case GRANIT_SHADER_TOOLS_ACCESS_READ_WRITE:
    return "read_write";
  default:
    return "unsupported";
  }
}

const char* scalar_type_name(uint32_t type) {
  switch (type) {
  case GRANIT_SHADER_TOOLS_SCALAR_FLOAT:
    return "float";
  case GRANIT_SHADER_TOOLS_SCALAR_SINT:
    return "sint";
  case GRANIT_SHADER_TOOLS_SCALAR_UINT:
    return "uint";
  default:
    return "unsupported";
  }
}

void print_interface_variable(const granit::shader_tools::interface_variable_info& variable) {
  std::cout << "{\"location\": " << variable.location << ", \"component\": " << variable.component
            << ", \"scalar_type\": " << json_string(scalar_type_name(variable.scalar_type))
            << ", \"bit_width\": " << variable.bit_width
            << ", \"vector_size\": " << variable.vector_size
            << ", \"name\": " << json_string(variable.name) << '}';
}

void print_json(const granit::shader_tools::result& result,
                const granit::shader_tools::result_info& info, const char* stage) {
  std::cout << "{\n  \"schema\": 1,\n  \"entry_point\": " << json_string(info.entry_point)
            << ",\n  \"stage\": " << json_string(stage) << ",\n  \"bindings\": [";
  for (uint64_t index = 0; index < result.binding_count(); ++index) {
    const auto [status, binding] = result.binding(index);
    if (status.failed())
      continue;
    std::cout << (index == 0 ? "\n" : ",\n") << "    {\"group\": " << binding.group
              << ", \"binding\": " << binding.binding
              << ", \"type\": " << json_string(binding_type_name(binding.type))
              << ", \"access\": " << json_string(binding_access_name(binding.access))
              << ", \"name\": " << json_string(binding.name)
              << ", \"array_count\": " << binding.array_count
              << ", \"minimum_binding_size\": " << binding.minimum_binding_size << '}';
  }
  std::cout << (result.binding_count() == 0 ? "" : "\n") << "  ],\n  \"vertex_inputs\": [";
  for (uint64_t index = 0; index < result.vertex_input_count(); ++index) {
    const auto [status, input] = result.vertex_input(index);
    if (status.failed())
      continue;
    std::cout << (index == 0 ? "\n    " : ",\n    ");
    print_interface_variable(input);
  }
  std::cout << (result.vertex_input_count() == 0 ? "" : "\n") << "  ],\n  \"fragment_outputs\": [";
  for (uint64_t index = 0; index < result.fragment_output_count(); ++index) {
    const auto [status, output] = result.fragment_output(index);
    if (status.failed())
      continue;
    std::cout << (index == 0 ? "\n    " : ",\n    ");
    print_interface_variable(output);
  }
  const auto workgroup = result.compute_workgroup_size();
  std::cout << (result.fragment_output_count() == 0 ? "" : "\n")
            << "  ],\n  \"workgroup_size\": {\"x\": " << workgroup.x << ", \"y\": " << workgroup.y
            << ", \"z\": " << workgroup.z << "},\n  \"overrides\": [";
  for (uint64_t index = 0; index < result.override_count(); ++index) {
    const auto [status, override_info] = result.override_at(index);
    if (status.failed())
      continue;
    std::cout << (index == 0 ? "\n" : ",\n") << "    {\"id\": " << override_info.id
              << ", \"scalar_type\": " << json_string(scalar_type_name(override_info.scalar_type))
              << ", \"bit_width\": " << override_info.bit_width
              << ", \"name\": " << json_string(override_info.name)
              << ", \"default_value\": " << override_info.default_value
              << ", \"default_value_size\": " << override_info.default_value_size << '}';
  }
  std::cout << (result.override_count() == 0 ? "" : "\n") << "  ]\n}\n";
}

int inspect_shader(const char* path, bool verify, bool json = false) {
  granit_shader_tools_inspect_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.input_path = path;
  desc.input_path_length = std::char_traits<char>::length(path);
  auto [status, result] = granit::shader_tools::inspect_spirv(desc);
  const auto info = result.info();
  const auto stage = info.stage == granit::shader_stage::vertex     ? "vertex"
                     : info.stage == granit::shader_stage::fragment ? "fragment"
                     : info.stage == granit::shader_stage::compute  ? "compute"
                                                                    : "unsupported";
  if (json && status.ok())
    print_json(result, info, stage);
  else if (verify && status.ok())
    std::cout << "SPIR-V 结构验证通过（" << info.entry_point << ", " << stage << "）\n";
  else
    std::cout << info.output;
  std::cerr << info.diagnostic;
  return status.ok() ? 0 : 1;
}

const char* asset_backend_name(uint32_t backend) {
  return backend == GRANIT_SHADER_BACKEND_VULKAN_BIT ? "vulkan" : "webgpu";
}

int print_target_capabilities(granit::shader_backend backend) {
  const auto [status, capabilities] = granit::shader_tools::target_capabilities(backend);
  if (status.failed()) {
    std::cerr << "不支持请求的 Shader 目标档位\n";
    return 1;
  }
  std::cout << "target=" << asset_backend_name(capabilities.backend) << "-portable\n"
            << "backend=" << asset_backend_name(capabilities.backend) << '\n'
            << "profile=portable\n"
            << "features=none\n";
  return 0;
}

void print_usage() {
  std::cerr << "用法：\n"
               "  granit_shader_tool inspect <shader.spv>\n"
               "  granit_shader_tool inspect --json <shader.spv>\n"
               "  granit_shader_tool verify <shader.spv>\n"
               "  granit_shader_tool targets\n"
               "  granit_shader_tool capabilities --target <vulkan-portable|webgpu-portable>\n"
               "  granit_shader_tool pack --spirv <shader.spv> --wgsl <shader.wgsl> "
               "--entry <name> --stage <vertex|fragment|compute> --asset <shader.grshader>\n"
               "  granit_shader_tool library --asset <shader.grshader>... "
               "--target <all|vulkan|webgpu> --output <shaders.grshlib>\n"
               "  granit_shader_tool asset-ids --asset <name=shader.grshader>... "
               "--output <shader-ids.inc>\n"
               "  granit_shader_tool compile --tint <path> --input <shader.wgsl> "
               "--entry <name> --stage <vertex|fragment|compute> --output <shader.spv> "
               "[--asset <shader.granit-shader> [--tint-revision <revision>] "
               "--asset-backend <all|vulkan|webgpu> "
               "--features <none|float16|subgroup>]\n";
  std::cerr << "  granit_shader_tool compile-hlsl --dxc <path> --tint <path> "
               "--input <shader.hlsl> --entry <name> --stage <vertex|fragment|compute> "
               "--spirv-output <shader.spv> --wgsl-output <shader.wgsl> "
               "[--define <NAME=VALUE>]... "
               "[--asset <shader.granit-shader> [--dxc-revision <revision>] "
               "[--tint-revision <revision>] --asset-backend <all|vulkan|webgpu>]\n";
}

} // namespace

int main(int argc, char** argv) {
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
    std::cerr << "未知 Shader 目标：" << target << '\n';
    return 2;
  }
  if (argc == 3 && std::string_view{argv[1]} == "inspect") {
    return inspect_shader(argv[2], false);
  }
  if (argc == 4 && std::string_view{argv[1]} == "inspect" && std::string_view{argv[2]} == "--json")
    return inspect_shader(argv[3], false, true);
  if (argc == 3 && std::string_view{argv[1]} == "verify") {
    return inspect_shader(argv[2], true);
  }
  if (argc >= 2 && std::string_view{argv[1]} == "compile")
    return compile_shader(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "pack")
    return pack_shader_asset(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "library")
    return link_shader_library(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "asset-ids")
    return emit_shader_asset_ids(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "compile-hlsl")
    return compile_hlsl_shader(argc, argv);
  print_usage();
  return 2;
}
