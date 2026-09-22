// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MATERIAL_HPP_
#define GRANIT_PIPELINE_MATERIAL_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/material.h>
#include <granit/renderer/pipeline_warmup.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/shader_library.hpp>
#include <granit/renderer/texture.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace granit {

[[nodiscard]] inline std::uint64_t material_parameter_id(std::string_view name) noexcept {
  if (name.size() > std::numeric_limits<std::uint32_t>::max())
    return 0;
  return granit_material_parameter_id(name.data(), static_cast<std::uint32_t>(name.size()));
}

enum class material_parameter_type : std::uint32_t {
  bool32 = GRANIT_MATERIAL_PARAMETER_BOOL32,
  int32 = GRANIT_MATERIAL_PARAMETER_INT32,
  uint32 = GRANIT_MATERIAL_PARAMETER_UINT32,
  float32 = GRANIT_MATERIAL_PARAMETER_FLOAT32,
  float2 = GRANIT_MATERIAL_PARAMETER_FLOAT2,
  float3 = GRANIT_MATERIAL_PARAMETER_FLOAT3,
  float4 = GRANIT_MATERIAL_PARAMETER_FLOAT4,
  matrix4 = GRANIT_MATERIAL_PARAMETER_MATRIX4,
  texture_view = GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW,
  sampler = GRANIT_MATERIAL_PARAMETER_SAMPLER,
};

/** 一项材质参数更新；引用的数据或资源必须保持到 update/initialize 调用返回。 */
class material_parameter_update {
public:
  [[nodiscard]] static constexpr material_parameter_update
  value(std::uint64_t id, material_parameter_type type, std::span<const std::byte> data) noexcept {
    return material_parameter_update{id, type, data, GRANIT_NULL_HANDLE};
  }

  [[nodiscard]] static constexpr material_parameter_update
  texture_binding(std::uint64_t id, texture_view_ref texture) noexcept {
    return material_parameter_update{
        id, material_parameter_type::texture_view, {}, texture.native_handle()};
  }

  [[nodiscard]] static constexpr material_parameter_update
  sampler_binding(std::uint64_t id, sampler_ref sampler) noexcept {
    return material_parameter_update{
        id, material_parameter_type::sampler, {}, sampler.native_handle()};
  }

  [[nodiscard]] constexpr granit_material_parameter_update native() const noexcept {
    return {.id = id_,
            .type = static_cast<granit_material_parameter_type>(type_),
            .reserved = 0,
            .data = data_.data(),
            .size = data_.size(),
            .resource = resource_};
  }

private:
  constexpr material_parameter_update(std::uint64_t id, material_parameter_type type,
                                      std::span<const std::byte> data,
                                      granit_handle resource) noexcept
      : id_(id), type_(type), data_(data), resource_(resource) {}

  std::uint64_t id_{};
  material_parameter_type type_{material_parameter_type::float32};
  std::span<const std::byte> data_;
  granit_handle resource_{GRANIT_NULL_HANDLE};
};

struct material_desc {
  std::span<const std::byte> archive;
  std::span<const material_parameter_update> initial_updates;
  shader_library_ref shader_library;
};

struct material_pipeline_warmup_desc {
  std::uint64_t pass{};
  std::uint64_t variant{};
  texture_format color_format{texture_format::undefined};
  texture_format depth_stencil_format{texture_format::undefined};
  sample_count samples{sample_count::one};
};

class material_instance;

/** 不拥有 Material，只在来源 Material 的有效期内使用。 */
class material_instance_ref {
public:
  material_instance_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_material native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr material_instance_ref
  from_native(granit_material handle) noexcept {
    return material_instance_ref{handle};
  }

private:
  friend class material_instance;

  explicit constexpr material_instance_ref(granit_material handle) noexcept : handle_(handle) {}

  granit_material handle_{GRANIT_NULL_HANDLE};
};

/** 公共 Material C ABI 的轻量 move-only RAII 包装。 */
class material_instance {
public:
  material_instance() = default;
  ~material_instance() { static_cast<void>(reset()); }
  material_instance(const material_instance&) = delete;
  material_instance& operator=(const material_instance&) = delete;
  material_instance(material_instance&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  material_instance& operator=(material_instance&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(renderer_ref owner,
                                  const granit_material_desc& desc) noexcept {
    const auto renderer = owner.native_handle();
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_material_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize(renderer& owner, const material_desc& desc) noexcept {
    if (desc.initial_updates.size() > std::numeric_limits<std::uint32_t>::max())
      return result::invalid_argument;
    try {
      std::vector<granit_material_parameter_update> updates;
      updates.reserve(desc.initial_updates.size());
      for (const auto& update : desc.initial_updates)
        updates.push_back(update.native());
      const granit_material_desc native{
          .struct_size = GRANIT_MATERIAL_DESC_VERSION_1_SIZE,
          .reserved = 0,
          .archive_data = desc.archive.data(),
          .archive_size = desc.archive.size(),
          .initial_updates = updates.data(),
          .initial_update_count = static_cast<std::uint32_t>(updates.size()),
          .reserved_tail = 0,
          .shader_library = desc.shader_library.native_handle(),
          .reserved_2 = 0,
      };
      return initialize(owner.ref(), native);
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result update(std::span<const material_parameter_update> updates) noexcept {
    if (updates.size() > std::numeric_limits<std::uint32_t>::max())
      return result::invalid_argument;
    try {
      std::vector<granit_material_parameter_update> native;
      native.reserve(updates.size());
      for (const auto& update : updates)
        native.push_back(update.native());
      return from_native(granit_material_update(renderer_, handle_, native.data(),
                                                static_cast<std::uint32_t>(native.size())));
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result add_pipeline_warmup(const material_pipeline_warmup_desc& desc,
                                           const pipeline_warmup_batch& batch,
                                           std::uint32_t& result_index) const noexcept {
    const granit_material_pipeline_warmup_desc native{
        .struct_size = sizeof(granit_material_pipeline_warmup_desc),
        .reserved = 0,
        .pass = desc.pass,
        .variant = desc.variant,
        .color_format = static_cast<granit_texture_format>(desc.color_format),
        .depth_stencil_format = static_cast<granit_texture_format>(desc.depth_stencil_format),
        .sample_count = static_cast<granit_sample_count>(desc.samples),
        .reserved_tail = 0,
    };
    return from_native(granit_material_add_pipeline_warmup(
        renderer_, handle_, &native, batch.native_handle(), &result_index));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    return from_native(granit_material_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr material_instance_ref ref() const noexcept {
    return material_instance_ref{handle_};
  }
  [[nodiscard]] granit_material native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_material handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
