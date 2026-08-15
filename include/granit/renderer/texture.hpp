// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEXTURE_HPP_
#define GRANIT_TEXTURE_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/resource_types.hpp>
#include <granit/renderer/texture.h>

namespace granit {

struct texture_desc {
  texture_dimension dimension{texture_dimension::two_dimensional};
  texture_format format{texture_format::undefined};
  texture_usage usage{};
  memory_location location{memory_location::automatic};
  std::uint32_t width{1};
  std::uint32_t height{1};
  std::uint32_t depth{1};
  std::uint32_t mip_levels{1};
  std::uint32_t array_layers{1};
  sample_count samples{sample_count::one};
};

struct texture_view_desc {
  texture_dimension dimension{texture_dimension::two_dimensional};
  texture_format format{texture_format::undefined};
  texture_aspect aspect{texture_aspect::automatic};
  std::uint32_t base_mip_level{};
  std::uint32_t mip_level_count{1};
  std::uint32_t base_array_layer{};
  std::uint32_t array_layer_count{1};
  component_swizzle red{component_swizzle::identity};
  component_swizzle green{component_swizzle::identity};
  component_swizzle blue{component_swizzle::identity};
  component_swizzle alpha{component_swizzle::identity};
};

struct texture_data_layout {
  std::uint64_t offset{};
  std::uint32_t bytes_per_row{};
  std::uint32_t rows_per_image{};
};

struct texture_write_region {
  std::uint32_t mip_level{};
  std::uint32_t base_array_layer{};
  std::uint32_t array_layer_count{1};
  texture_aspect aspect{texture_aspect::color};
  std::uint32_t x{};
  std::uint32_t y{};
  std::uint32_t z{};
  std::uint32_t width{1};
  std::uint32_t height{1};
  std::uint32_t depth{1};
};

/** 无异常、move-only 的 Texture RAII 包装。 */
class texture {
public:
  texture() = default;
  ~texture() { static_cast<void>(reset()); }
  texture(const texture&) = delete;
  texture& operator=(const texture&) = delete;
  texture(texture&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  texture& operator=(texture&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer, const texture_desc& desc) noexcept {
    if (valid() || renderer == GRANIT_NULL_HANDLE)
      return result::invalid_argument;
    const granit_texture_desc native{.struct_size = GRANIT_TEXTURE_DESC_VERSION_1_SIZE,
                                     .dimension = static_cast<std::uint32_t>(desc.dimension),
                                     .format = static_cast<std::uint32_t>(desc.format),
                                     .usage = static_cast<std::uint32_t>(desc.usage),
                                     .memory_location = static_cast<std::uint32_t>(desc.location),
                                     .width = desc.width,
                                     .height = desc.height,
                                     .depth = desc.depth,
                                     .mip_levels = desc.mip_levels,
                                     .array_layers = desc.array_layers,
                                     .sample_count = static_cast<std::uint32_t>(desc.samples),
                                     .reserved = 0};
    const auto value = granit_texture_create(renderer, &native, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_texture_destroy(renderer, handle));
  }
  [[nodiscard]] result write(std::span<const std::byte> data, const texture_data_layout& layout,
                             const texture_write_region& region) noexcept {
    if (!valid() || data.empty())
      return result::invalid_argument;
    const granit_texture_data_layout native_layout{.offset = layout.offset,
                                                   .bytes_per_row = layout.bytes_per_row,
                                                   .rows_per_image = layout.rows_per_image};
    const granit_texture_write_region native_region{.mip_level = region.mip_level,
                                                    .base_array_layer = region.base_array_layer,
                                                    .array_layer_count = region.array_layer_count,
                                                    .aspect =
                                                        static_cast<std::uint32_t>(region.aspect),
                                                    .x = region.x,
                                                    .y = region.y,
                                                    .z = region.z,
                                                    .width = region.width,
                                                    .height = region.height,
                                                    .depth = region.depth};
    return from_native(granit_texture_write(renderer_, handle_, data.data(), data.size(),
                                            &native_layout, &native_region));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_texture native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_texture handle_{GRANIT_NULL_HANDLE};
};

/** 无异常、move-only 的 Texture View RAII 包装。 */
class texture_view {
public:
  texture_view() = default;
  ~texture_view() { static_cast<void>(reset()); }
  texture_view(const texture_view&) = delete;
  texture_view& operator=(const texture_view&) = delete;
  texture_view(texture_view&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  texture_view& operator=(texture_view&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer, granit_texture texture,
                                  const texture_view_desc& desc = {}) noexcept {
    if (valid() || renderer == GRANIT_NULL_HANDLE || texture == GRANIT_NULL_HANDLE)
      return result::invalid_argument;
    const granit_texture_view_desc native{
        .struct_size = GRANIT_TEXTURE_VIEW_DESC_VERSION_1_SIZE,
        .dimension = static_cast<std::uint32_t>(desc.dimension),
        .format = static_cast<std::uint32_t>(desc.format),
        .reserved = 0,
        .range = {.aspect = static_cast<std::uint32_t>(desc.aspect),
                  .base_mip_level = desc.base_mip_level,
                  .mip_level_count = desc.mip_level_count,
                  .base_array_layer = desc.base_array_layer,
                  .array_layer_count = desc.array_layer_count},
        .components = {.red = static_cast<std::uint32_t>(desc.red),
                       .green = static_cast<std::uint32_t>(desc.green),
                       .blue = static_cast<std::uint32_t>(desc.blue),
                       .alpha = static_cast<std::uint32_t>(desc.alpha)}};
    const auto value = granit_texture_view_create(renderer, texture, &native, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_texture_view_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_texture_view native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_texture_view handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
