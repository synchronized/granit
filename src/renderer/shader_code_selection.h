// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_SHADER_CODE_SELECTION_H_
#define GRANIT_RENDERER_SHADER_CODE_SELECTION_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <granit/renderer/renderer.h>
#include <granit/renderer/shader.h>

namespace granit::detail {

struct selected_shader_code {
  granit_shader_code_format format{GRANIT_SHADER_CODE_FORMAT_SPIRV};
  std::span<const std::byte> bytes;
};

inline granit_result select_portable_shader_code(granit_renderer renderer,
                                                 std::span<const std::byte> spirv,
                                                 std::string_view wgsl,
                                                 selected_shader_code& selected) noexcept {
  granit_renderer_shader_capabilities capabilities = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
  const auto result = granit_renderer_get_shader_capabilities(renderer, &capabilities);
  if (result != GRANIT_SUCCESS)
    return result;
  if (capabilities.backend == GRANIT_RENDERER_BACKEND_WEBGPU) {
    selected.format = GRANIT_SHADER_CODE_FORMAT_WGSL;
    selected.bytes = std::as_bytes(std::span{wgsl.data(), wgsl.size()});
  } else {
    selected.format = GRANIT_SHADER_CODE_FORMAT_SPIRV;
    selected.bytes = spirv;
  }
  return GRANIT_SUCCESS;
}

/** 内部嵌入代码按 Renderer 选择单一格式；公共材质路径应使用 Shader Library。 */
inline granit_result
create_shader_from_portable_code(granit_renderer renderer, granit_shader_stage stage,
                                 std::span<const std::byte> spirv, std::string_view wgsl,
                                 std::string_view entry_point, granit_shader& shader) noexcept {
  selected_shader_code selected;
  const auto result = select_portable_shader_code(renderer, spirv, wgsl, selected);
  if (result != GRANIT_SUCCESS)
    return result;

  granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  desc.stage = stage;
  desc.entry_point = entry_point.data();
  desc.entry_point_length = static_cast<std::uint32_t>(entry_point.size());
  desc.code_format = selected.format;
  desc.code = selected.bytes.data();
  desc.code_size = selected.bytes.size();
  return granit_shader_create(renderer, &desc, &shader);
}

} // namespace granit::detail

#endif
