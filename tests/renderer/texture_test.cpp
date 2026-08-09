// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>
#include <granit/renderer.hpp>
#include <granit/texture.hpp>

namespace {
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device || value == granit::result::unsupported;
}

TEST_CASE("Texture 与默认 View 支持独立和级联销毁", "[texture]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-texture-tests"});
  if (unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
  desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  desc.width = 32;
  desc.height = 32;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &desc, &texture,
                                                  &view) == GRANIT_SUCCESS);
  REQUIRE(texture != GRANIT_NULL_HANDLE);
  REQUIRE(view != GRANIT_NULL_HANDLE);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
  CHECK(granit_texture_view_destroy(renderer.native_handle(), view) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Texture View 校验格式和 Renderer 归属", "[texture][validation]") {
  granit::renderer first;
  const auto result = first.initialize({.application_name = "granit-texture-first"});
  if (unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);
  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-texture-second"}) ==
          granit::result::success);
  granit::texture texture;
  REQUIRE(texture.initialize(first.native_handle(), {.format = granit::texture_format::rgba8_unorm,
                                                     .usage = granit::texture_usage::sampled,
                                                     .width = 16,
                                                     .height = 16}) == granit::result::success);
  granit_texture_view_desc view_desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
  view_desc.format = GRANIT_TEXTURE_FORMAT_BGRA8_UNORM;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  CHECK(granit_texture_view_create(first.native_handle(), texture.native_handle(), &view_desc,
                                   &view) == GRANIT_ERROR_UNSUPPORTED);
  view_desc.format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  CHECK(granit_texture_view_create(second.native_handle(), texture.native_handle(), &view_desc,
                                   &view) == GRANIT_ERROR_INVALID_HANDLE);
  granit::texture_view cpp_view;
  REQUIRE(cpp_view.initialize(first.native_handle(), texture.native_handle()) ==
          granit::result::success);
  REQUIRE(cpp_view.reset() == granit::result::success);
}

TEST_CASE("验证模式允许 Texture 级联销毁用户 View", "[texture][lifecycle]") {
  granit::renderer renderer;
  const auto result = renderer.initialize(
      {.application_name = "granit-texture-lifecycle-tests", .enable_validation = true});
  if (unavailable(result))
    SKIP("当前运行环境不支持 Vulkan 验证层或没有满足要求的设备");
  REQUIRE(result == granit::result::success);

  granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
  desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &desc, &texture,
                                                  &view) == GRANIT_SUCCESS);

  CHECK(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
  CHECK(granit_texture_view_destroy(renderer.native_handle(), view) == GRANIT_ERROR_INVALID_HANDLE);
}
} // namespace
