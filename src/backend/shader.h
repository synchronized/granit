// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_SHADER_H_
#define GRANIT_BACKEND_SHADER_H_

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include <granit/renderer/shader.h>

#include "backend/resources.h"

namespace granit::detail {

/** 提供使用可移植源码创建 Shader 的后端能力。 */
class backend_shader_renderer {
public:
  backend_shader_renderer() = default;
  virtual ~backend_shader_renderer() = default;
  backend_shader_renderer(const backend_shader_renderer&) = delete;
  backend_shader_renderer& operator=(const backend_shader_renderer&) = delete;

  [[nodiscard]] virtual std::unique_ptr<backend_shader_resource> allocate_shader_resource() = 0;
  [[nodiscard]] virtual granit_result create_wgsl_shader(backend_shader_resource& shader,
                                                         granit_shader_stage stage,
                                                         std::string_view source,
                                                         std::string_view entry_point) noexcept = 0;
};

/** 提供使用 SPIR-V 创建 Shader 的后端能力。 */
class backend_spirv_shader_renderer {
public:
  backend_spirv_shader_renderer() = default;
  virtual ~backend_spirv_shader_renderer() = default;
  backend_spirv_shader_renderer(const backend_spirv_shader_renderer&) = delete;
  backend_spirv_shader_renderer& operator=(const backend_spirv_shader_renderer&) = delete;

  [[nodiscard]] virtual std::unique_ptr<backend_shader_resource> allocate_shader_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_spirv_shader(backend_shader_resource& shader, granit_shader_stage stage,
                      std::span<const std::uint32_t> code,
                      std::string_view entry_point) noexcept = 0;
};

} // namespace granit::detail

#endif
