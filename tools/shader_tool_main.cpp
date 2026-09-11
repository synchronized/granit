// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/sha256.h"
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
  struct owned_object {
    std::vector<std::byte> manifest;
    std::vector<std::byte> wgsl;
    std::vector<std::byte> spirv;
  };
  std::vector<owned_object> owned;
  owned.reserve(object_paths.size());
  for (const auto& object_path : object_paths) {
    owned_object object{.manifest = read_bytes(object_path), .wgsl = {}, .spirv = {}};
    if ((backend_mask & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0)
      object.wgsl = read_bytes(object_path + ".wgsl");
    if ((backend_mask & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0)
      object.spirv = read_bytes(object_path + ".spv");
    if (object.manifest.empty() ||
        ((backend_mask & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0 && object.wgsl.empty()) ||
        ((backend_mask & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0 && object.spirv.empty())) {
      std::cerr << "无法读取 Shader Object或目标 sidecar：" << object_path << '\n';
      return 1;
    }
    owned.push_back(std::move(object));
  }
  std::vector<granit::detail::shader_format::shader_library_object_source> sources;
  sources.reserve(owned.size());
  for (const auto& object : owned)
    sources.push_back({object.manifest, object.wgsl, object.spirv});
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

int build_shader_object(int argc, char** argv) {
  const auto spirv_path = option_value(argc, argv, "--spirv");
  const auto wgsl_path = option_value(argc, argv, "--wgsl");
  const auto entry = option_value(argc, argv, "--entry");
  const auto stage = option_value(argc, argv, "--stage");
  const auto object_path = option_value(argc, argv, "--output");
  if (!spirv_path || !wgsl_path || !entry || !stage || !object_path ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute")) {
    std::cerr << "object 需要 --spirv、--wgsl、--entry、--stage 和 --output\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? granit::shader_stage::vertex
                           : *stage == "fragment" ? granit::shader_stage::fragment
                                                  : granit::shader_stage::compute;
  constexpr std::string_view identity = "import-v1";
  constexpr std::string_view target = "portable";
  constexpr std::string_view options = "validated-pair";
  const granit_shader_tools_object_desc desc{
      .struct_size = sizeof(granit_shader_tools_object_desc),
      .source_path = wgsl_path->data(),
      .source_path_length = wgsl_path->size(),
      .source_language = GRANIT_SHADER_SOURCE_LANGUAGE_WGSL,
      .wgsl_path = wgsl_path->data(),
      .wgsl_path_length = wgsl_path->size(),
      .spirv_path = spirv_path->data(),
      .spirv_path_length = spirv_path->size(),
      .output_path = object_path->data(),
      .output_path_length = object_path->size(),
      .tint_revision = identity.data(),
      .tint_revision_length = identity.size(),
      .target_environment = target.data(),
      .target_environment_length = target.size(),
      .compile_options = options.data(),
      .compile_options_length = options.size(),
      .backend_mask = GRANIT_SHADER_BACKEND_ALL_BITS,
      .required_features = 0,
      .entry_point = entry->data(),
      .entry_point_length = entry->size(),
      .stage = static_cast<granit_shader_stage>(stage_value),
      .reserved = 0,
  };
  const auto [status, cache_hit] = granit::shader_tools::build_object(desc);
  if (status.failed()) {
    std::cerr << "无法构建 Shader Object\n";
    return 1;
  }
  std::cout << (cache_hit ? "Shader Object 未变化：" : "已构建 Shader Object：") << *object_path
            << '\n';
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
        separator != std::string::npos && separator != 0 && separator + 1 < spec.size() &&
        ((name.front() >= 'a' && name.front() <= 'z') ||
         (name.front() >= 'A' && name.front() <= 'Z') || name.front() == '_') &&
        std::ranges::all_of(name, [](const unsigned char value) {
          return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                 (value >= '0' && value <= '9') || value == '_';
        });
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
    content << "constexpr std::array<std::byte, 32> " << name << "{\n";
    for (const auto value : object.content_id)
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

const char* binding_type_name(granit::shader_tools::binding_type type) {
  switch (type) {
  case granit::shader_tools::binding_type::uniform_buffer:
    return "uniform_buffer";
  case granit::shader_tools::binding_type::storage_buffer:
    return "storage_buffer";
  case granit::shader_tools::binding_type::sampled_texture:
    return "sampled_texture";
  case granit::shader_tools::binding_type::storage_texture:
    return "storage_texture";
  case granit::shader_tools::binding_type::sampler:
    return "sampler";
  default:
    return "unsupported";
  }
}

const char* binding_access_name(granit::shader_tools::binding_access access) {
  switch (access) {
  case granit::shader_tools::binding_access::read:
    return "read";
  case granit::shader_tools::binding_access::write:
    return "write";
  case granit::shader_tools::binding_access::read_write:
    return "read_write";
  default:
    return "unsupported";
  }
}

const char* scalar_type_name(granit::shader_tools::scalar_type type) {
  switch (type) {
  case granit::shader_tools::scalar_type::floating_point:
    return "float";
  case granit::shader_tools::scalar_type::signed_integer:
    return "sint";
  case granit::shader_tools::scalar_type::unsigned_integer:
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

void print_json(const granit::shader_tools::reflection& result,
                const granit::shader_tools::reflection_info& info, const char* stage) {
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

const char* object_backend_name(uint32_t backend) {
  return backend == GRANIT_SHADER_BACKEND_VULKAN_BIT ? "vulkan" : "webgpu";
}

int print_target_capabilities(granit::shader_backend backend) {
  const auto [status, capabilities] = granit::shader_tools::target_capabilities(backend);
  if (status.failed()) {
    std::cerr << "不支持请求的 Shader 目标档位\n";
    return 1;
  }
  std::cout << "target=" << object_backend_name(capabilities.backend) << "-portable\n"
            << "backend=" << object_backend_name(capabilities.backend) << '\n'
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
               "  granit_shader_tool object --spirv <shader.spv> --wgsl <shader.wgsl> "
               "--entry <name> --stage <vertex|fragment|compute> --output <shader.grshaderobj>\n"
               "  granit_shader_tool library --object <shader.grshaderobj>... "
               "--target <all|vulkan|webgpu> --output <shaders.grshlib>\n"
               "  granit_shader_tool object-ids --object <name=shader.grshaderobj>... "
               "--output <shader-ids.inc>\n"
               "  granit_shader_tool compile --tint <path> --input <shader.wgsl> "
               "--entry <name> --stage <vertex|fragment|compute> --output <shader.spv> "
               "[--object <shader.grshaderobj> [--tint-revision <revision>] "
               "--object-backend <all|vulkan|webgpu> "
               "--features <none|float16|subgroup>]\n";
  std::cerr << "  granit_shader_tool compile-hlsl --dxc <path> --tint <path> "
               "--input <shader.hlsl> --entry <name> --stage <vertex|fragment|compute> "
               "--spirv-output <shader.spv> --wgsl-output <shader.wgsl> "
               "[--define <NAME=VALUE>]... "
               "[--object <shader.grshaderobj> [--dxc-revision <revision>] "
               "[--tint-revision <revision>] --object-backend <all|vulkan|webgpu>]\n";
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
  if (argc >= 2 && std::string_view{argv[1]} == "object")
    return build_shader_object(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "library")
    return link_shader_library(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "object-ids")
    return emit_shader_object_ids(argc, argv);
  if (argc >= 2 && std::string_view{argv[1]} == "compile-hlsl")
    return compile_hlsl_shader(argc, argv);
  print_usage();
  return 2;
}
