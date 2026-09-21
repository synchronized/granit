// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_TEXT_ATLAS_HPP_
#define GRANIT_PIPELINE_TEXT_ATLAS_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/text_atlas.h>
#include <granit/renderer/renderer.hpp>

#include <cstdint>
#include <span>
#include <utility>

namespace granit {

struct text_atlas_desc {
  std::uint32_t page_width{512};
  std::uint32_t page_height{512};
  std::uint32_t max_pages{8};
  std::uint32_t padding{1};
};

struct text_glyph_bitmap_desc {
  std::uint32_t glyph_id{};
  std::uint64_t font_key{};
  std::uint32_t width{};
  std::uint32_t height{};
  float bearing_x{};
  float bearing_y{};
  std::span<const std::uint8_t> bitmap;
  std::uint32_t bytes_per_row{};
};

struct text_atlas_stats {
  std::uint32_t glyph_count{};
  std::uint32_t page_count{};
};

class text_atlas;

/** 不拥有 Text Atlas，只在来源 Atlas 的有效期内使用。 */
class text_atlas_ref {
public:
  text_atlas_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_text_atlas native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr text_atlas_ref from_native(granit_text_atlas handle) noexcept {
    return text_atlas_ref{handle};
  }

private:
  friend class text_atlas;
  explicit constexpr text_atlas_ref(granit_text_atlas handle) noexcept : handle_(handle) {}
  granit_text_atlas handle_{GRANIT_NULL_HANDLE};
};

/** 公共 Text Atlas C ABI 的轻量 move-only RAII 包装。 */
class text_atlas {
public:
  text_atlas() = default;
  ~text_atlas() { static_cast<void>(destroy()); }
  text_atlas(const text_atlas&) = delete;
  text_atlas& operator=(const text_atlas&) = delete;
  text_atlas(text_atlas&& other) noexcept { move_from(other); }
  text_atlas& operator=(text_atlas&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      move_from(other);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_text_atlas_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_text_atlas_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize(renderer& owner, const text_atlas_desc& desc = {}) noexcept {
    const granit_text_atlas_desc native{
        .struct_size = GRANIT_TEXT_ATLAS_DESC_VERSION_1_SIZE,
        .page_width = desc.page_width,
        .page_height = desc.page_height,
        .max_pages = desc.max_pages,
        .padding = desc.padding,
        .reserved = {},
    };
    return initialize(owner.native_handle(), native);
  }
  [[nodiscard]] result upload_glyph(const granit_text_glyph_bitmap_desc& glyph) noexcept {
    return from_native(granit_text_atlas_upload_glyph(renderer_, handle_, &glyph));
  }
  [[nodiscard]] result upload_glyph(const text_glyph_bitmap_desc& glyph) noexcept {
    const granit_text_glyph_bitmap_desc native{
        .struct_size = GRANIT_TEXT_GLYPH_BITMAP_DESC_VERSION_1_SIZE,
        .glyph_id = glyph.glyph_id,
        .font_key = glyph.font_key,
        .width = glyph.width,
        .height = glyph.height,
        .bearing_x = glyph.bearing_x,
        .bearing_y = glyph.bearing_y,
        .bitmap = glyph.bitmap.data(),
        .bitmap_size = static_cast<std::uint64_t>(glyph.bitmap.size()),
        .bytes_per_row = glyph.bytes_per_row,
        .reserved = {},
    };
    return upload_glyph(native);
  }
  [[nodiscard]] result get_stats(granit_text_atlas_stats& stats) const noexcept {
    return from_native(granit_text_atlas_get_stats(renderer_, handle_, &stats));
  }
  [[nodiscard]] result get_stats(text_atlas_stats& stats) const noexcept {
    granit_text_atlas_stats native = GRANIT_TEXT_ATLAS_STATS_INIT;
    const auto value = get_stats(native);
    if (value.ok())
      stats = {.glyph_count = native.glyph_count, .page_count = native.page_count};
    return value;
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_text_atlas_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr text_atlas_ref ref() const noexcept { return text_atlas_ref{handle_}; }
  [[nodiscard]] granit_text_atlas native_handle() const noexcept { return handle_; }

private:
  void move_from(text_atlas& other) noexcept {
    renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
    handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
  }

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_text_atlas handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
