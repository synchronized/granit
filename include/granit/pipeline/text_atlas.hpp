// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_TEXT_ATLAS_HPP_
#define GRANIT_PIPELINE_TEXT_ATLAS_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/text_atlas.h>

#include <utility>

namespace granit {

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
    if (succeeded(value))
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result upload_glyph(const granit_text_glyph_bitmap_desc& glyph) noexcept {
    return from_native(granit_text_atlas_upload_glyph(renderer_, handle_, &glyph));
  }
  [[nodiscard]] result get_stats(granit_text_atlas_stats& stats) const noexcept {
    return from_native(granit_text_atlas_get_stats(renderer_, handle_, &stats));
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_text_atlas_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
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
