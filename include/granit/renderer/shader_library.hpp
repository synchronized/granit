// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_LIBRARY_HPP_
#define GRANIT_SHADER_LIBRARY_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/shader_library.h>

namespace granit {

enum class shader_library_backend : std::uint32_t {
  vulkan = GRANIT_SHADER_LIBRARY_BACKEND_VULKAN_BIT,
  webgpu = GRANIT_SHADER_LIBRARY_BACKEND_WEBGPU_BIT,
};

struct shader_library_info {
  std::uint32_t backend_flags{};
  shader_digest content_digest{};
  std::uint32_t shader_count{};
  std::uint32_t variant_count{};
  std::uint32_t payload_count{};
  std::uint64_t archive_size{};
};

namespace detail {

inline void copy_shader_library_info(const granit_shader_library_info& source,
                                     shader_library_info& destination) noexcept {
  destination.backend_flags = source.backend_flags;
  std::memcpy(destination.content_digest.data(), source.content_digest,
              destination.content_digest.size());
  destination.shader_count = source.shader_count;
  destination.variant_count = source.variant_count;
  destination.payload_count = source.payload_count;
  destination.archive_size = source.archive_size;
}

} // namespace detail

/** 校验内存中的 `.grshlib` 并复制其稳定摘要。 */
[[nodiscard]] inline result inspect_shader_library(std::span<const std::byte> archive,
                                                   shader_library_info& info) noexcept {
  if (archive.empty())
    return result::invalid_argument;
  granit_shader_library_info native = GRANIT_SHADER_LIBRARY_INFO_INIT;
  const auto value =
      from_native(granit_shader_library_inspect(archive.data(), archive.size(), &native));
  if (value.ok())
    detail::copy_shader_library_info(native, info);
  return value;
}

/**
 * Shader Library 的移动独占包装。
 *
 * initialize 成功后，archive 的地址和内容必须保持有效且不变，直到 reset 或对象析构。
 */
class shader_library {
public:
  shader_library() = default;
  ~shader_library() { static_cast<void>(reset()); }
  shader_library(const shader_library&) = delete;
  shader_library& operator=(const shader_library&) = delete;
  shader_library(shader_library&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  shader_library& operator=(shader_library&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  std::span<const std::byte> archive) noexcept {
    if (valid() || archive.empty())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_shader_library_desc desc{.struct_size = GRANIT_SHADER_LIBRARY_DESC_VERSION_1_SIZE,
                                          .reserved = 0,
                                          .archive_data = archive.data(),
                                          .archive_size = archive.size()};
    const auto value = from_native(granit_shader_library_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }

  [[nodiscard]] result get_info(shader_library_info& info) const noexcept {
    if (!valid())
      return result::invalid_handle;
    granit_shader_library_info native = GRANIT_SHADER_LIBRARY_INFO_INIT;
    const auto value = from_native(granit_shader_library_get_info(renderer_, handle_, &native));
    if (value.ok())
      detail::copy_shader_library_info(native, info);
    return value;
  }

  /** 按内容 ID 选择当前 Renderer 支持的变体并创建 Shader。Library 必须比 Shader 更晚销毁。 */
  [[nodiscard]] result create_shader(const shader_content_id& content_id,
                                     shader& destination) const noexcept {
    if (!valid())
      return result::invalid_handle;
    if (destination.valid())
      return result::invalid_argument;
    const auto value = from_native(granit_shader_create_from_library(
        renderer_, handle_, reinterpret_cast<const std::uint8_t*>(content_id.data()),
        &destination.handle_));
    if (value.ok())
      destination.renderer_ = renderer_;
    return value;
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto value = from_native(granit_shader_library_destroy(renderer_, handle_));
    if (value.ok()) {
      renderer_ = GRANIT_NULL_HANDLE;
      handle_ = GRANIT_NULL_HANDLE;
    }
    return value;
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_shader_library native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_shader_library handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
