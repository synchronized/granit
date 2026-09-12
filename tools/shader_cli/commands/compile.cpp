// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/arguments.h"
#include "shader_cli/commands.h"
#include <granit/tools/shader_tools.hpp>

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::shader_cli {
namespace {
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

std::optional<std::string> resolve_tool_identity(std::string_view path,
                                                 const std::optional<std::string>& supplied) {
  if (supplied)
    return supplied;
  auto [status, digest] = granit::shader_tools::tool_identity(path);
  if (status.failed())
    return std::nullopt;
  return digest;
}

} // namespace

int compile_shader(int argc, char** argv) {
  const auto tint = option_value(argc, argv, "--tint");
  const auto input = option_value(argc, argv, "--input");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto output = option_value(argc, argv, "--output");
  const auto object = option_value(argc, argv, "--object");
  const auto tint_revision = option_value(argc, argv, "--tint-revision");
  const auto target_environment = option_value(argc, argv, "--target-environment");
  const auto object_backend = option_value(argc, argv, "--object-backend");
  const auto features = option_value(argc, argv, "--features");
  if (!tint || !input || !entry || !stage || !output ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute") ||
      (object_backend && !object) || (features && !object) ||
      (object_backend && *object_backend != "all" && *object_backend != "vulkan" &&
       *object_backend != "webgpu") ||
      (features && *features != "none" && *features != "float16" && *features != "subgroup")) {
    std::cerr << "compile 需要 --tint、--input、--entry、--stage 和 --output\n";
    std::cerr << "可选 --tint-revision、--target-environment、"
                 "--object-backend <all|vulkan|webgpu> 和 --features <none|float16|subgroup>\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? GRANIT_SHADER_STAGE_VERTEX
                           : *stage == "fragment" ? GRANIT_SHADER_STAGE_FRAGMENT
                                                  : GRANIT_SHADER_STAGE_COMPUTE;
  constexpr std::string_view default_target = "vulkan1.3";
  constexpr std::string_view compile_options = "format=spirv;validate=1";
  const auto target = target_environment ? std::string_view{*target_environment} : default_target;
  const auto backend_mask = !object_backend || *object_backend == "all"
                                ? GRANIT_SHADER_BACKEND_ALL_BITS
                            : *object_backend == "vulkan" ? GRANIT_SHADER_BACKEND_VULKAN_BIT
                                                          : GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  const auto required_features = !features || *features == "none" ? UINT64_C(0)
                                 : *features == "float16" ? GRANIT_SHADER_FEATURE_FLOAT16_BIT
                                                          : GRANIT_SHADER_FEATURE_SUBGROUP_BIT;
  if (required_features != 0) {
    std::cerr << "目标 portable 档位不支持必需特性：" << *features << '\n';
    return 1;
  }
  const auto tint_identity =
      object ? resolve_tool_identity(*tint, tint_revision) : std::optional<std::string>{""};
  if (!tint_identity) {
    std::cerr << "无法读取 Tint 工具身份\n";
    return 1;
  }
  if (object) {
    granit_shader_tools_object_cache_desc cache{};
    cache.struct_size = sizeof(cache);
    cache.source_path = input->data();
    cache.source_path_length = input->size();
    cache.source_language = GRANIT_SHADER_SOURCE_LANGUAGE_WGSL;
    cache.spirv_output_path = output->data();
    cache.spirv_output_path_length = output->size();
    cache.object_path = object->data();
    cache.object_path_length = object->size();
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
    const auto [cache_status, cache_hit] = granit::shader_tools::restore_object_cache(cache);
    if (cache_status.failed()) {
      std::cerr << "Shader Object 缓存查询失败\n";
      return 1;
    }
    if (cache_hit) {
      std::cout << "Shader Object 缓存命中：" << *object << '\n';
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
  if (status.ok() && object) {
    granit_shader_tools_object_desc object_desc{};
    object_desc.struct_size = sizeof(object_desc);
    object_desc.source_path = input->data();
    object_desc.source_path_length = input->size();
    object_desc.source_language = GRANIT_SHADER_SOURCE_LANGUAGE_WGSL;
    object_desc.wgsl_path = input->data();
    object_desc.wgsl_path_length = input->size();
    object_desc.spirv_path = output->data();
    object_desc.spirv_path_length = output->size();
    object_desc.output_path = object->data();
    object_desc.output_path_length = object->size();
    object_desc.tint_revision = tint_identity->data();
    object_desc.tint_revision_length = tint_identity->size();
    object_desc.target_environment = target.data();
    object_desc.target_environment_length = target.size();
    object_desc.compile_options = compile_options.data();
    object_desc.compile_options_length = compile_options.size();
    object_desc.backend_mask = backend_mask;
    object_desc.required_features = required_features;
    object_desc.entry_point = entry->data();
    object_desc.entry_point_length = entry->size();
    object_desc.stage = stage_value;
    const auto [object_status, cache_hit] = result.write_object(object_desc);
    if (object_status.failed()) {
      std::cerr << "Shader Object写入失败\n";
      return 1;
    }
    std::cout << (cache_hit ? "Shader Object 缓存命中：" : "已生成 Shader Object：") << *object
              << '\n';
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
  const auto object = option_value(argc, argv, "--object");
  const auto dxc_revision = option_value(argc, argv, "--dxc-revision");
  const auto tint_revision = option_value(argc, argv, "--tint-revision");
  const auto object_backend = option_value(argc, argv, "--object-backend");
  std::vector<std::pair<std::string, std::string>> definitions;
  const auto define_arguments = option_values(argc, argv, "--define");
  if (!dxc || !tint || !input || !entry || !stage || !spirv_output || !wgsl_output ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute") ||
      (object_backend && !object) ||
      (object_backend && *object_backend != "all" && *object_backend != "vulkan" &&
       *object_backend != "webgpu") ||
      !parse_defines(define_arguments, definitions)) {
    std::cerr << "compile-hlsl 需要 --dxc、--tint、--input、--entry、--stage、"
                 "--spirv-output 和 --wgsl-output\n";
    std::cerr << "可选 --dxc-revision 与 --tint-revision 可覆盖自动二进制身份\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? GRANIT_SHADER_STAGE_VERTEX
                           : *stage == "fragment" ? GRANIT_SHADER_STAGE_FRAGMENT
                                                  : GRANIT_SHADER_STAGE_COMPUTE;
  const auto backend_mask = !object_backend || *object_backend == "all"
                                ? GRANIT_SHADER_BACKEND_ALL_BITS
                            : *object_backend == "vulkan" ? GRANIT_SHADER_BACKEND_VULKAN_BIT
                                                          : GRANIT_SHADER_BACKEND_WEBGPU_BIT;
  const auto dxc_identity =
      object ? resolve_tool_identity(*dxc, dxc_revision) : std::optional<std::string>{""};
  const auto tint_identity =
      object ? resolve_tool_identity(*tint, tint_revision) : std::optional<std::string>{""};
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
  if (object && backend_mask == GRANIT_SHADER_BACKEND_ALL_BITS) {
    granit_shader_tools_object_cache_desc cache{};
    cache.struct_size = sizeof(cache);
    cache.source_path = input->data();
    cache.source_path_length = input->size();
    cache.source_language = GRANIT_SHADER_SOURCE_LANGUAGE_HLSL;
    cache.wgsl_output_path = wgsl_output->data();
    cache.wgsl_output_path_length = wgsl_output->size();
    cache.spirv_output_path = spirv_output->data();
    cache.spirv_output_path_length = spirv_output->size();
    cache.object_path = object->data();
    cache.object_path_length = object->size();
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
    const auto [cache_status, cache_hit] = granit::shader_tools::restore_object_cache(cache);
    if (cache_status.failed()) {
      std::cerr << "HLSL Shader Object 缓存查询失败\n";
      return 1;
    }
    if (cache_hit) {
      std::cout << "Shader Object 缓存命中：" << *object << '\n';
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
  if (status.failed() || !object)
    return status.ok() ? 0 : 1;

  granit_shader_tools_object_desc object_desc{};
  object_desc.struct_size = sizeof(object_desc);
  object_desc.source_path = input->data();
  object_desc.source_path_length = input->size();
  object_desc.source_language = GRANIT_SHADER_SOURCE_LANGUAGE_HLSL;
  object_desc.wgsl_path = wgsl_output->data();
  object_desc.wgsl_path_length = wgsl_output->size();
  object_desc.spirv_path = spirv_output->data();
  object_desc.spirv_path_length = spirv_output->size();
  object_desc.output_path = object->data();
  object_desc.output_path_length = object->size();
  object_desc.tint_revision = revisions.data();
  object_desc.tint_revision_length = revisions.size();
  object_desc.target_environment = target.data();
  object_desc.target_environment_length = target.size();
  object_desc.compile_options = options.data();
  object_desc.compile_options_length = options.size();
  object_desc.backend_mask = backend_mask;
  object_desc.entry_point = entry->data();
  object_desc.entry_point_length = entry->size();
  object_desc.stage = stage_value;
  const auto [object_status, cache_hit] = result.write_object(object_desc);
  if (object_status.failed()) {
    std::cerr << "HLSL Shader Object写入失败\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader Object 内容未变化：" : "已生成 Shader Object：") << *object
            << '\n';
  return 0;
}

} // namespace granit::shader_cli
