// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_SHADER_ASSET_FILE_H_
#define GRANIT_TESTS_SUPPORT_SHADER_ASSET_FILE_H_

#include <granit/renderer/shader.hpp>

#include "shader_format/shader_object.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace granit::tests {

inline bool read_shader_bytes(const std::filesystem::path& path, std::vector<std::byte>& output) {
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  output.resize(bytes.size());
  if (!bytes.empty())
    std::memcpy(output.data(), bytes.data(), bytes.size());
  return !stream.bad() && !bytes.empty();
}

struct shader_asset_payload {
  shader_stage stage{shader_stage::vertex};
  shader_code_format code_format{shader_code_format::spirv};
  std::string entry_point;
  std::vector<std::byte> code;
};

// Shader Object 只在工具链测试中使用；测试层负责选择并校验后端 payload。
inline result read_shader_asset(granit_renderer renderer, const std::filesystem::path& path,
                                shader_asset_payload& output) {
  granit_renderer_shader_capabilities capabilities = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
  const auto status = granit_renderer_get_shader_capabilities(renderer, &capabilities);
  if (status != GRANIT_SUCCESS)
    return from_native(status);
  std::vector<std::byte> manifest;
  if (!read_shader_bytes(path, manifest))
    return result::invalid_argument;
  granit::detail::shader_format::shader_object_view object;
  if (granit::detail::shader_format::decode_shader_object(manifest, object) !=
      granit::detail::shader_format::shader_object_error::success)
    return result::invalid_argument;
  const auto backend = capabilities.backend == GRANIT_RENDERER_BACKEND_VULKAN
                           ? granit::detail::shader_format::shader_object_backend::vulkan
                           : granit::detail::shader_format::shader_object_backend::webgpu;
  const auto* variant = granit::detail::shader_format::find_shader_object_variant(
      object, backend, granit::shader_profile::portable);
  if (variant == nullptr || (variant->required_features & ~capabilities.supported_features) != 0)
    return result::unsupported;
  const auto suffix =
      backend == granit::detail::shader_format::shader_object_backend::vulkan ? ".spv" : ".wgsl";
  if (!read_shader_bytes(path.string() + suffix, output.code) ||
      granit::detail::shader_format::validate_shader_object_payload(object, backend, output.code) !=
          granit::detail::shader_format::shader_object_error::success)
    return result::invalid_argument;
  output.stage = object.stage;
  output.code_format = variant->code_format;
  output.entry_point = object.entry_point;
  return result::success;
}

inline result load_shader_asset(granit_renderer renderer, const std::filesystem::path& path,
                                shader& output) {
  shader_asset_payload payload;
  const auto status = read_shader_asset(renderer, path, payload);
  if (status != result::success)
    return status;
  return output.initialize(renderer, {.stage = payload.stage,
                                      .code_format = payload.code_format,
                                      .code = payload.code,
                                      .entry_point = payload.entry_point});
}

inline result load_shader_asset(granit_renderer renderer, const std::filesystem::path& path,
                                granit_shader& output) {
  shader_asset_payload payload;
  const auto status = read_shader_asset(renderer, path, payload);
  if (status != result::success)
    return status;
  const granit_shader_desc desc{
      .struct_size = GRANIT_SHADER_DESC_SIZE,
      .stage = static_cast<granit_shader_stage>(payload.stage),
      .code_format = static_cast<granit_shader_code_format>(payload.code_format),
      .reserved = 0,
      .code = payload.code.data(),
      .code_size = payload.code.size(),
      .entry_point = payload.entry_point.data(),
      .entry_point_length = static_cast<std::uint32_t>(payload.entry_point.size()),
      .reserved_2 = 0};
  return from_native(granit_shader_create(renderer, &desc, &output));
}

} // namespace granit::tests

#endif
