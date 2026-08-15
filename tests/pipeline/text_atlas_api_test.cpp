// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_atlas.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>

namespace {
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable || value == granit::result::not_ready;
}

granit_text_glyph_bitmap_desc glyph_desc(uint64_t font_key, uint32_t glyph_id,
                                         const std::array<uint8_t, 8>& bitmap) {
  granit_text_glyph_bitmap_desc desc = GRANIT_TEXT_GLYPH_BITMAP_DESC_INIT;
  desc.font_key = font_key;
  desc.glyph_id = glyph_id;
  desc.width = 4;
  desc.height = 2;
  desc.bearing_x = 1;
  desc.bearing_y = -2;
  desc.bitmap = bitmap.data();
  desc.bitmap_size = bitmap.size();
  desc.bytes_per_row = 4;
  return desc;
}
} // namespace

TEST_CASE("Text Atlas缓存R8字形并按需分页") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-text-atlas"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  granit_text_atlas_desc desc = GRANIT_TEXT_ATLAS_DESC_INIT;
  desc.page_width = 8;
  desc.page_height = 8;
  desc.max_pages = 2;
  granit::text_atlas atlas;
  REQUIRE(atlas.initialize(renderer.native_handle(), desc) == granit::result::success);
  constexpr std::array<uint8_t, 8> bitmap{0, 32, 64, 96, 128, 160, 192, 255};
  auto first = glyph_desc(1, 10, bitmap);
  auto second = glyph_desc(1, 11, bitmap);
  auto third = glyph_desc(2, 10, bitmap);
  REQUIRE(atlas.upload_glyph(first) == granit::result::success);
  REQUIRE(atlas.upload_glyph(second) == granit::result::success);
  REQUIRE(atlas.upload_glyph(third) == granit::result::success);
  granit_text_atlas_stats stats = GRANIT_TEXT_ATLAS_STATS_INIT;
  REQUIRE(atlas.get_stats(stats) == granit::result::success);
  CHECK(stats.glyph_count == 3);
  CHECK(stats.page_count == 2);

  REQUIRE(atlas.upload_glyph(first) == granit::result::success);
  REQUIRE(atlas.get_stats(stats) == granit::result::success);
  CHECK(stats.glyph_count == 3);
  CHECK(stats.page_count == 2);
  first.bearing_x = 2;
  CHECK(atlas.upload_glyph(first) == granit::result::invalid_argument);

  granit_text_glyph_bitmap_desc space = GRANIT_TEXT_GLYPH_BITMAP_DESC_INIT;
  space.font_key = 1;
  space.glyph_id = 32;
  REQUIRE(atlas.upload_glyph(space) == granit::result::success);
  REQUIRE(atlas.get_stats(stats) == granit::result::success);
  CHECK(stats.glyph_count == 4);
  CHECK(stats.page_count == 2);

  auto invalid = glyph_desc(0, 12, bitmap);
  CHECK(atlas.upload_glyph(invalid) == granit::result::invalid_argument);
  invalid = glyph_desc(1, 12, bitmap);
  invalid.bitmap_size = 7;
  CHECK(atlas.upload_glyph(invalid) == granit::result::invalid_argument);

  granit::renderer second_renderer;
  REQUIRE(second_renderer.initialize({.application_name = "granit-text-atlas-second"}) ==
          granit::result::success);
  CHECK(granit_text_atlas_get_stats(second_renderer.native_handle(), atlas.native_handle(),
                                    &stats) == GRANIT_ERROR_INVALID_HANDLE);
  const auto old = atlas.native_handle();
  REQUIRE(atlas.destroy() == granit::result::success);
  CHECK(granit_text_atlas_destroy(renderer.native_handle(), old) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Text Atlas在页数耗尽时明确失败") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-text-atlas-full"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  granit_text_atlas_desc desc = GRANIT_TEXT_ATLAS_DESC_INIT;
  desc.page_width = 8;
  desc.page_height = 8;
  desc.max_pages = 1;
  granit::text_atlas atlas;
  REQUIRE(atlas.initialize(renderer.native_handle(), desc) == granit::result::success);
  constexpr std::array<uint8_t, 8> bitmap{};
  REQUIRE(atlas.upload_glyph(glyph_desc(1, 1, bitmap)) == granit::result::success);
  REQUIRE(atlas.upload_glyph(glyph_desc(1, 2, bitmap)) == granit::result::success);
  CHECK(atlas.upload_glyph(glyph_desc(1, 3, bitmap)) == granit::result::out_of_memory);
}
