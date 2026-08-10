// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_HPP_
#define GRANIT_SHADER_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include <granit/result.hpp>
#include <granit/shader.h>

namespace granit {

enum class shader_stage : std::uint32_t {
  vertex = GRANIT_SHADER_STAGE_VERTEX,
  fragment = GRANIT_SHADER_STAGE_FRAGMENT,
  compute = GRANIT_SHADER_STAGE_COMPUTE,
};

struct shader_desc {
  shader_stage stage{shader_stage::vertex};
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

  [[nodiscard]] result initialize(granit_renderer renderer, const shader_desc& desc) noexcept {
    if (valid() || renderer == GRANIT_NULL_HANDLE || desc.entry_point.size() > UINT32_MAX)
      return result::invalid_argument;
    const granit_shader_desc native{.struct_size = GRANIT_SHADER_DESC_VERSION_1_SIZE,
                                    .stage = static_cast<granit_shader_stage>(desc.stage),
                                    .code = desc.code.data(),
                                    .code_size = desc.code.size(),
                                    .entry_point = desc.entry_point.data(),
                                    .entry_point_length =
                                        static_cast<std::uint32_t>(desc.entry_point.size()),
                                    .reserved = 0};
    const auto value = granit_shader_create(renderer, &native, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
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
  [[nodiscard]] granit_shader native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_shader handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
