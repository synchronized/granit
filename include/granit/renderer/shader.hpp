// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_HPP_
#define GRANIT_SHADER_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/core/shader_types.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/shader.h>

namespace granit {

class shader_library;
class shader;

/** 不拥有 Shader，只在来源 Shader 的有效期内使用。 */
class shader_ref {
public:
  shader_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_shader native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr shader_ref from_native(granit_shader handle) noexcept {
    return shader_ref{handle};
  }

private:
  friend class shader;

  explicit constexpr shader_ref(granit_shader handle) noexcept : handle_(handle) {}

  granit_shader handle_{GRANIT_NULL_HANDLE};
};

struct shader_desc {
  shader_stage stage{shader_stage::vertex};
  shader_code_format code_format{shader_code_format::spirv};
  std::span<const std::byte> code;
  std::string_view entry_point{"main"};
};

class shader {
public:
  shader() = default;
  ~shader() { static_cast<void>(reset()); }
  shader(const shader&) = delete;
  shader& operator=(const shader&) = delete;
  shader(shader&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  shader& operator=(shader&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(renderer_ref owner, const shader_desc& desc) noexcept {
    const auto renderer = owner.native_handle();
    if (valid() || desc.entry_point.size() > UINT32_MAX)
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_shader_desc native{
        .struct_size = GRANIT_SHADER_DESC_SIZE,
        .stage = static_cast<granit_shader_stage>(desc.stage),
        .code_format = static_cast<granit_shader_code_format>(desc.code_format),
        .reserved = 0,
        .code = desc.code.data(),
        .code_size = desc.code.size(),
        .entry_point = desc.entry_point.data(),
        .entry_point_length = static_cast<std::uint32_t>(desc.entry_point.size()),
        .reserved_2 = 0};
    return initialize_native(renderer, native);
  }

  [[nodiscard]] result initialize(renderer& owner, const shader_desc& desc) noexcept {
    return initialize(owner.ref(), desc);
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_shader_destroy(renderer, handle));
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr shader_ref ref() const noexcept { return shader_ref{handle_}; }
  [[nodiscard]] granit_shader native_handle() const noexcept { return handle_; }

private:
  friend class shader_library;

  [[nodiscard]] result initialize_native(granit_renderer renderer,
                                         const granit_shader_desc& native) noexcept {
    const auto value = granit_shader_create(renderer, &native, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }

  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_shader handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
