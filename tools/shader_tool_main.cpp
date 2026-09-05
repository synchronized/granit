// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.hpp>

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::optional<std::string> option_value(int argc, char** argv, std::string_view name) {
  for (int index = 2; index + 1 < argc; ++index) {
    if (argv[index] == name)
      return argv[index + 1];
  }
  return std::nullopt;
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
      (asset && !tint_revision) || (asset_backend && !asset) || (features && !asset) ||
      (asset_backend && *asset_backend != "all" && *asset_backend != "vulkan" &&
       *asset_backend != "webgpu") ||
      (features && *features != "none" && *features != "float16" && *features != "subgroup")) {
    std::cerr << "compile 需要 --tint、--input、--entry、--stage 和 --output\n";
    std::cerr << "使用 --asset 时还需要 --tint-revision；可选 --target-environment 和 "
                 "--asset-backend <all|vulkan|webgpu> 和 --features <none|float16|subgroup>\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? GRANIT_SHADER_TOOLS_STAGE_VERTEX
                           : *stage == "fragment" ? GRANIT_SHADER_TOOLS_STAGE_FRAGMENT
                                                  : GRANIT_SHADER_TOOLS_STAGE_COMPUTE;
  constexpr std::string_view default_target = "vulkan1.3";
  constexpr std::string_view compile_options = "format=spirv;validate=1";
  const auto target = target_environment ? std::string_view{*target_environment} : default_target;
  const auto backend_mask = !asset_backend || *asset_backend == "all"
                                ? GRANIT_SHADER_TOOLS_ASSET_BACKEND_ALL
                            : *asset_backend == "vulkan" ? GRANIT_SHADER_TOOLS_ASSET_BACKEND_VULKAN
                                                         : GRANIT_SHADER_TOOLS_ASSET_BACKEND_WEBGPU;
  const auto required_features = !features || *features == "none" ? UINT64_C(0)
                                 : *features == "float16" ? GRANIT_SHADER_FEATURE_FLOAT16_BIT
                                                          : GRANIT_SHADER_FEATURE_SUBGROUP_BIT;
  if (required_features != 0) {
    std::cerr << "目标 portable 档位不支持必需特性：" << *features << '\n';
    return 1;
  }
  if (asset) {
    granit_shader_tools_cache_desc cache{};
    cache.struct_size = sizeof(cache);
    cache.wgsl_path = input->data();
    cache.wgsl_path_length = input->size();
    cache.spirv_output_path = output->data();
    cache.spirv_output_path_length = output->size();
    cache.asset_path = asset->data();
    cache.asset_path_length = asset->size();
    cache.entry_point = entry->data();
    cache.entry_point_length = entry->size();
    cache.stage = stage_value;
    cache.tint_revision = tint_revision->data();
    cache.tint_revision_length = tint_revision->size();
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
  granit_shader_tools_compile_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.tint_path = tint->data();
  desc.tint_path_length = tint->size();
  desc.input_path = input->data();
  desc.input_path_length = input->size();
  desc.entry_point = entry->data();
  desc.entry_point_length = entry->size();
  desc.stage = stage_value;
  desc.output_path = output->data();
  desc.output_path_length = output->size();
  auto [status, result] = granit::shader_tools::compile_wgsl(desc);
  const auto info = result.info();
  std::cout << info.output;
  std::cerr << info.diagnostic;
  if (status.ok() && asset) {
    granit_shader_tools_asset_desc asset_desc{};
    asset_desc.struct_size = sizeof(asset_desc);
    asset_desc.wgsl_path = input->data();
    asset_desc.wgsl_path_length = input->size();
    asset_desc.spirv_path = output->data();
    asset_desc.spirv_path_length = output->size();
    asset_desc.output_path = asset->data();
    asset_desc.output_path_length = asset->size();
    asset_desc.tint_revision = tint_revision->data();
    asset_desc.tint_revision_length = tint_revision->size();
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
  if (!dxc || !tint || !input || !entry || !stage || !spirv_output || !wgsl_output ||
      (*stage != "vertex" && *stage != "fragment" && *stage != "compute") ||
      (asset && (!dxc_revision || !tint_revision)) || (asset_backend && !asset) ||
      (asset_backend && *asset_backend != "all" && *asset_backend != "vulkan" &&
       *asset_backend != "webgpu")) {
    std::cerr << "compile-hlsl 需要 --dxc、--tint、--input、--entry、--stage、"
                 "--spirv-output 和 --wgsl-output\n";
    std::cerr << "使用 --asset 时还需要 --dxc-revision 与 --tint-revision\n";
    return 2;
  }
  const auto stage_value = *stage == "vertex"     ? GRANIT_SHADER_TOOLS_STAGE_VERTEX
                           : *stage == "fragment" ? GRANIT_SHADER_TOOLS_STAGE_FRAGMENT
                                                  : GRANIT_SHADER_TOOLS_STAGE_COMPUTE;
  granit_shader_tools_hlsl_compile_desc desc{};
  desc.struct_size = sizeof(desc);
  desc.dxc_path = dxc->data();
  desc.dxc_path_length = dxc->size();
  desc.tint_path = tint->data();
  desc.tint_path_length = tint->size();
  desc.input_path = input->data();
  desc.input_path_length = input->size();
  desc.entry_point = entry->data();
  desc.entry_point_length = entry->size();
  desc.stage = stage_value;
  desc.spirv_output_path = spirv_output->data();
  desc.spirv_output_path_length = spirv_output->size();
  desc.wgsl_output_path = wgsl_output->data();
  desc.wgsl_output_path_length = wgsl_output->size();
  auto [status, result] = granit::shader_tools::compile_hlsl(desc);
  const auto info = result.info();
  std::cout << info.output;
  std::cerr << info.diagnostic;
  if (status.failed() || !asset)
    return status.ok() ? 0 : 1;

  const auto backend_mask = !asset_backend || *asset_backend == "all"
                                ? GRANIT_SHADER_TOOLS_ASSET_BACKEND_ALL
                            : *asset_backend == "vulkan" ? GRANIT_SHADER_TOOLS_ASSET_BACKEND_VULKAN
                                                         : GRANIT_SHADER_TOOLS_ASSET_BACKEND_WEBGPU;
  const std::string revisions = "dxc=" + *dxc_revision + ";tint=" + *tint_revision;
  constexpr std::string_view target = "vulkan1.3+webgpu-portable";
  constexpr std::string_view options = "source=hlsl;spirv=vulkan1.3;bridge=spirv1.3";
  granit_shader_tools_asset_desc asset_desc{};
  asset_desc.struct_size = sizeof(asset_desc);
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
  const auto stage = info.stage == GRANIT_SHADER_TOOLS_STAGE_VERTEX     ? "vertex"
                     : info.stage == GRANIT_SHADER_TOOLS_STAGE_FRAGMENT ? "fragment"
                     : info.stage == GRANIT_SHADER_TOOLS_STAGE_COMPUTE  ? "compute"
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
  return backend == GRANIT_SHADER_TOOLS_ASSET_BACKEND_VULKAN ? "vulkan" : "webgpu";
}

int print_target_capabilities(uint32_t backend) {
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
               "  granit_shader_tool compile --tint <path> --input <shader.wgsl> "
               "--entry <name> --stage <vertex|fragment|compute> --output <shader.spv> "
               "[--asset <shader.granit-shader> --tint-revision <revision> "
               "--asset-backend <all|vulkan|webgpu> "
               "--features <none|float16|subgroup>]\n";
  std::cerr << "  granit_shader_tool compile-hlsl --dxc <path> --tint <path> "
               "--input <shader.hlsl> --entry <name> --stage <vertex|fragment|compute> "
               "--spirv-output <shader.spv> --wgsl-output <shader.wgsl> "
               "[--asset <shader.granit-shader> --dxc-revision <revision> "
               "--tint-revision <revision> --asset-backend <all|vulkan|webgpu>]\n";
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
      return print_target_capabilities(GRANIT_SHADER_TOOLS_ASSET_BACKEND_VULKAN);
    if (target == "webgpu-portable")
      return print_target_capabilities(GRANIT_SHADER_TOOLS_ASSET_BACKEND_WEBGPU);
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
  if (argc >= 2 && std::string_view{argv[1]} == "compile-hlsl")
    return compile_hlsl_shader(argc, argv);
  print_usage();
  return 2;
}
