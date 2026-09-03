// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_TARGET_HPP_
#define GRANIT_RENDER_TARGET_HPP_

#include <cstdint>
#include <span>

#include <granit/renderer/render_target.h>

namespace granit {

enum class attachment_load_operation : std::uint32_t {
  undefined = GRANIT_ATTACHMENT_LOAD_OPERATION_UNDEFINED,
  load = GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD,
  clear = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR,
  discard = GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD,
};

enum class attachment_store_operation : std::uint32_t {
  undefined = GRANIT_ATTACHMENT_STORE_OPERATION_UNDEFINED,
  store = GRANIT_ATTACHMENT_STORE_OPERATION_STORE,
  discard = GRANIT_ATTACHMENT_STORE_OPERATION_DISCARD,
};

struct clear_color_value {
  float red{};
  float green{};
  float blue{};
  float alpha{1.0F};
};

struct clear_depth_stencil_value {
  float depth{1.0F};
  std::uint32_t stencil{};
};

struct color_attachment_desc {
  granit_texture_view view{GRANIT_NULL_HANDLE};
  granit_texture_view resolve_view{GRANIT_NULL_HANDLE};
  attachment_load_operation load_operation{attachment_load_operation::clear};
  attachment_store_operation store_operation{attachment_store_operation::store};
  clear_color_value clear_value{};

  [[nodiscard]] constexpr granit_color_attachment_desc native() const noexcept {
    return {
        .struct_size = GRANIT_COLOR_ATTACHMENT_DESC_VERSION_2_SIZE,
        .load_operation = static_cast<granit_attachment_load_operation>(load_operation),
        .store_operation = static_cast<granit_attachment_store_operation>(store_operation),
        .reserved = 0,
        .view = view,
        .clear_value = {clear_value.red, clear_value.green, clear_value.blue, clear_value.alpha},
        .reserved_2 = 0,
        .resolve_view = resolve_view};
  }
};

struct depth_stencil_attachment_desc {
  granit_texture_view view{GRANIT_NULL_HANDLE};
  attachment_load_operation depth_load_operation{attachment_load_operation::clear};
  attachment_store_operation depth_store_operation{attachment_store_operation::store};
  attachment_load_operation stencil_load_operation{attachment_load_operation::discard};
  attachment_store_operation stencil_store_operation{attachment_store_operation::discard};
  clear_depth_stencil_value clear_value{};

  [[nodiscard]] constexpr granit_depth_stencil_attachment_desc native() const noexcept {
    return {
        .struct_size = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_VERSION_1_SIZE,
        .depth_load_operation = static_cast<granit_attachment_load_operation>(depth_load_operation),
        .depth_store_operation =
            static_cast<granit_attachment_store_operation>(depth_store_operation),
        .stencil_load_operation =
            static_cast<granit_attachment_load_operation>(stencil_load_operation),
        .stencil_store_operation =
            static_cast<granit_attachment_store_operation>(stencil_store_operation),
        .reserved = 0,
        .view = view,
        .clear_value = {clear_value.depth, clear_value.stencil},
        .reserved_2 = 0,
    };
  }
};

struct rendering_area {
  std::uint32_t x{};
  std::uint32_t y{};
  std::uint32_t width{};
  std::uint32_t height{};
};

struct rendering_desc {
  std::span<const color_attachment_desc> color_attachments;
  const depth_stencil_attachment_desc* depth_stencil_attachment{};
  rendering_area area{};
  std::uint32_t layer_count{1};
};

static_assert(sizeof(granit_color_attachment_desc) == GRANIT_COLOR_ATTACHMENT_DESC_VERSION_2_SIZE);
static_assert(sizeof(granit_depth_stencil_attachment_desc) ==
              GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_VERSION_1_SIZE);
static_assert(sizeof(granit_rendering_desc) == GRANIT_RENDERING_DESC_VERSION_1_SIZE);

} // namespace granit

#endif
