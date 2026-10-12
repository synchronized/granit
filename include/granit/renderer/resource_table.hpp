// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RESOURCE_TABLE_HPP_
#define GRANIT_RESOURCE_TABLE_HPP_

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <granit/core/result.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/resource_table.h>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.hpp>

namespace granit {

enum class resource_table_binding_mode : std::uint32_t {
  automatic = GRANIT_RESOURCE_TABLE_BINDING_MODE_AUTO,
  explicit_selection = GRANIT_RESOURCE_TABLE_BINDING_MODE_EXPLICIT,
};

enum class resource_table_resource_type : std::uint32_t {
  sampled_texture_view = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW,
  sampler = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER,
  storage_buffer = GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER,
};

enum class resource_table_support : std::uint32_t {
  unsupported = GRANIT_RESOURCE_TABLE_SUPPORT_UNSUPPORTED,
  logical = GRANIT_RESOURCE_TABLE_SUPPORT_LOGICAL,
  native = GRANIT_RESOURCE_TABLE_SUPPORT_NATIVE,
  fallback = GRANIT_RESOURCE_TABLE_SUPPORT_FALLBACK,
};

struct resource_table_capabilities {
  resource_table_support support{resource_table_support::unsupported};
  std::uint32_t resource_types{};
  std::uint32_t max_sampled_texture_views{};
  std::uint32_t max_samplers{};
  std::uint32_t max_storage_buffers{};
};

[[nodiscard]] inline result get_resource_table_capabilities(
    renderer_ref owner, resource_table_capabilities& capabilities) noexcept {
  granit_resource_table_capabilities native = GRANIT_RESOURCE_TABLE_CAPABILITIES_INIT;
  const auto value = from_native(granit_renderer_get_resource_table_capabilities(
      owner.native_handle(), &native));
  if (value.failed()) return value;
  capabilities = {.support = static_cast<resource_table_support>(native.support),
                  .resource_types = native.resource_types,
                  .max_sampled_texture_views = native.max_sampled_texture_views,
                  .max_samplers = native.max_samplers,
                  .max_storage_buffers = native.max_storage_buffers};
  return result::success;
}

[[nodiscard]] inline result get_resource_table_capabilities(
    renderer& owner, resource_table_capabilities& capabilities) noexcept {
  return get_resource_table_capabilities(owner.ref(), capabilities);
}

struct resource_table_desc {
  resource_table_binding_mode binding_mode{resource_table_binding_mode::automatic};
  std::uint32_t resource_types{};
  std::uint32_t sampled_texture_view_capacity{};
  std::uint32_t sampler_capacity{};
  std::uint32_t storage_buffer_capacity{};
};

struct resource_table_update {
  resource_table_resource_type type{};
  std::uint32_t slot{};
  granit_handle resource{GRANIT_NULL_HANDLE};
  std::uint64_t offset{};
  std::uint64_t size{};
};

class resource_table;

/** 不拥有 Resource Table，只在来源对象有效期内使用。 */
class resource_table_ref {
public:
  resource_table_ref() = default;
  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr granit_resource_table native_handle() const noexcept { return handle_; }

private:
  friend class resource_table;
  explicit constexpr resource_table_ref(granit_resource_table handle) noexcept : handle_(handle) {}
  granit_resource_table handle_{GRANIT_NULL_HANDLE};
};

class resource_table {
public:
  resource_table() = default;
  ~resource_table() { static_cast<void>(reset()); }
  resource_table(const resource_table&) = delete;
  resource_table& operator=(const resource_table&) = delete;
  resource_table(resource_table&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  resource_table& operator=(resource_table&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(renderer_ref owner, const resource_table_desc& desc) noexcept {
    if (valid()) return result::invalid_argument;
    const auto renderer = owner.native_handle();
    if (renderer == GRANIT_NULL_HANDLE) return result::invalid_handle;
    const granit_resource_table_desc native{
        .struct_size = GRANIT_RESOURCE_TABLE_DESC_VERSION_1_SIZE,
        .binding_mode = static_cast<std::uint32_t>(desc.binding_mode),
        .resource_types = desc.resource_types,
        .sampled_texture_view_capacity = desc.sampled_texture_view_capacity,
        .sampler_capacity = desc.sampler_capacity,
        .storage_buffer_capacity = desc.storage_buffer_capacity,
        .reserved = {0, 0}};
    const auto value = granit_resource_table_create(renderer, &native, &handle_);
    if (value == GRANIT_SUCCESS) renderer_ = renderer;
    return from_native(value);
  }

  [[nodiscard]] result initialize(renderer& owner, const resource_table_desc& desc) noexcept {
    return initialize(owner.ref(), desc);
  }

  [[nodiscard]] result update(const resource_table_update& update_value) noexcept {
    if (!valid()) return result::invalid_handle;
    const granit_resource_table_update_desc native{
        .type = static_cast<std::uint32_t>(update_value.type),
        .slot = update_value.slot,
        .resource = update_value.resource,
        .offset = update_value.offset,
        .size = update_value.size};
    return from_native(granit_resource_table_update(renderer_, handle_, &native));
  }

  [[nodiscard]] result update_batch(std::span<const resource_table_update> updates) noexcept {
    if (!valid()) return result::invalid_handle;
    std::vector<granit_resource_table_update_desc> native;
    native.reserve(updates.size());
    for (const auto& value : updates) {
      native.push_back({.type = static_cast<std::uint32_t>(value.type),
                        .slot = value.slot,
                        .resource = value.resource,
                        .offset = value.offset,
                        .size = value.size});
    }
    return from_native(granit_resource_table_update_batch(
        renderer_, handle_, native.data(), static_cast<std::uint32_t>(native.size())));
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid()) return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_resource_table_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr resource_table_ref ref() const noexcept { return resource_table_ref{handle_}; }
  [[nodiscard]] granit_resource_table native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_resource_table handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
