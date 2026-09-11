// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_HPP_
#define GRANIT_SHADER_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <granit/core/result.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/shader.h>
#include <granit/renderer/shader_library.hpp>

namespace granit {

static_assert(GRANIT_SHADER_ASSET_ID_SIZE == GRANIT_SHADER_LIBRARY_CONTENT_DIGEST_SIZE);

enum class shader_stage : std::uint32_t {
  vertex = GRANIT_SHADER_STAGE_VERTEX,
  fragment = GRANIT_SHADER_STAGE_FRAGMENT,
  compute = GRANIT_SHADER_STAGE_COMPUTE,
};

enum class shader_code_format : std::uint32_t {
  wgsl = GRANIT_SHADER_CODE_FORMAT_WGSL,
  spirv = GRANIT_SHADER_CODE_FORMAT_SPIRV,
};

struct shader_desc {
  shader_stage stage{shader_stage::vertex};
  shader_code_format code_format{shader_code_format::spirv};
  std::span<const std::byte> code;
  std::string_view entry_point{"main"};
};

/** 由 ShaderTools 生成的清单及当前后端 sidecar 字节。 */
struct packaged_shader_asset_desc {
  std::span<const std::byte> manifest;
  std::span<const std::byte> sidecar;
};

struct shader_asset_variant_info {
  renderer_backend backend{renderer_backend::automatic};
  shader_code_format code_format{shader_code_format::wgsl};
  std::uint32_t profile{};
  std::uint64_t required_features{};
  std::uint64_t payload_size{};
  std::array<std::byte, GRANIT_SHADER_ASSET_ID_SIZE> payload_digest{};
};

struct shader_asset_info {
  shader_content_id content_id{};
  std::array<std::byte, GRANIT_SHADER_ASSET_ID_SIZE> cache_key{};
  shader_stage stage{shader_stage::vertex};
  std::string entry_point;
  std::vector<shader_asset_variant_info> variants;
};

/** 校验内存中的 `.grshader` 清单，并复制其稳定元数据。 */
[[nodiscard]] inline result inspect_shader_asset(std::span<const std::byte> manifest,
                                                 shader_asset_info& info) noexcept {
  if (manifest.empty())
    return result::invalid_argument;
  granit_shader_asset_info native = GRANIT_SHADER_ASSET_INFO_INIT;
  auto value = from_native(granit_shader_asset_inspect(manifest.data(), manifest.size(), &native));
  if (value.failed())
    return value;
  try {
    std::string entry_point(native.entry_point_length, '\0');
    native.entry_point = entry_point.data();
    native.entry_point_capacity = static_cast<std::uint32_t>(entry_point.size() + 1);
    value = from_native(granit_shader_asset_inspect(manifest.data(), manifest.size(), &native));
    if (value.failed())
      return value;
    shader_asset_info replacement;
    std::memcpy(replacement.content_id.data(), native.content_id, replacement.content_id.size());
    std::memcpy(replacement.cache_key.data(), native.cache_key, replacement.cache_key.size());
    replacement.stage = static_cast<shader_stage>(native.stage);
    replacement.entry_point = std::move(entry_point);
    replacement.variants.reserve(native.variant_count);
    for (std::uint32_t index = 0; index < native.variant_count; ++index) {
      const auto& source = native.variants[index];
      shader_asset_variant_info variant{
          .backend = static_cast<renderer_backend>(source.backend),
          .code_format = static_cast<shader_code_format>(source.code_format),
          .profile = source.profile,
          .required_features = source.required_features,
          .payload_size = source.payload_size,
      };
      std::memcpy(variant.payload_digest.data(), source.payload_digest,
                  variant.payload_digest.size());
      replacement.variants.push_back(variant);
    }
    info = std::move(replacement);
    return result::success;
  } catch (const std::bad_alloc&) {
    return result::out_of_memory;
  } catch (...) {
    return result::internal;
  }
}

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

  [[nodiscard]] result initialize_packaged_asset(granit_renderer renderer,
                                                 const packaged_shader_asset_desc& desc) noexcept {
    if (valid() || desc.manifest.empty() || desc.sidecar.empty())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_shader_asset_desc native{.struct_size = GRANIT_SHADER_ASSET_DESC_SIZE,
                                          .reserved = 0,
                                          .manifest_data = desc.manifest.data(),
                                          .manifest_size = desc.manifest.size(),
                                          .sidecar_data = desc.sidecar.data(),
                                          .sidecar_size = desc.sidecar.size()};
    const auto value = from_native(granit_shader_create_from_asset(renderer, &native, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }

  [[nodiscard]] result initialize_library(granit_renderer renderer, granit_shader_library library,
                                          const shader_content_id& content_id) noexcept {
    if (valid())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE || library == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const auto value = from_native(granit_shader_create_from_library(
        renderer, library, reinterpret_cast<const std::uint8_t*>(content_id.data()), &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
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
