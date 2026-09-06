// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include <array>
#include <cstring>
#include <future>
#include <vector>

namespace {

TEST_CASE("Texture View创建把空资源句柄归类为无效句柄", "[texture][contract]") {
  const granit_texture_view_desc desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
  granit_texture_view view = UINT64_C(1);
  CHECK(granit_texture_view_create(GRANIT_NULL_HANDLE, UINT64_C(1), &desc, &view) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(view == GRANIT_NULL_HANDLE);
  CHECK(granit_texture_view_create(UINT64_C(1), GRANIT_NULL_HANDLE, &desc, &view) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(view == GRANIT_NULL_HANDLE);

  granit::texture_view cpp_view;
  CHECK(cpp_view.initialize(GRANIT_NULL_HANDLE, UINT64_C(1)) == granit::result::invalid_handle);
  CHECK(cpp_view.initialize(UINT64_C(1), GRANIT_NULL_HANDLE) == granit::result::invalid_handle);
}
bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device || value == granit::result::unsupported;
}

TEST_CASE("Texture Format Footprint返回后端无关的紧密块信息", "[texture][format]") {
  granit_texture_format_footprint native = GRANIT_TEXTURE_FORMAT_FOOTPRINT_INIT;
  CHECK(granit_texture_format_get_footprint(GRANIT_TEXTURE_FORMAT_R8_UNORM, &native) ==
        GRANIT_SUCCESS);
  CHECK(native.block_width == 1);
  CHECK(native.block_height == 1);
  CHECK(native.bytes_per_block == 1);
  CHECK(granit_texture_format_get_footprint(GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT, &native) ==
        GRANIT_SUCCESS);
  CHECK(native.bytes_per_block == 8);
  CHECK(granit_texture_format_get_footprint(GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT, &native) ==
        GRANIT_SUCCESS);
  CHECK(native.bytes_per_block == 8);
  CHECK(granit_texture_format_get_footprint(GRANIT_TEXTURE_FORMAT_BC1_RGBA_SRGB, &native) ==
        GRANIT_SUCCESS);
  CHECK(native.block_width == 4);
  CHECK(native.block_height == 4);
  CHECK(native.bytes_per_block == 8);
  CHECK(granit_texture_format_get_footprint(GRANIT_TEXTURE_FORMAT_BC7_RGBA_UNORM, &native) ==
        GRANIT_SUCCESS);
  CHECK(native.bytes_per_block == 16);
  CHECK(granit_texture_format_get_footprint(GRANIT_TEXTURE_FORMAT_UNDEFINED, &native) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_texture_format_get_footprint(GRANIT_TEXTURE_FORMAT_R8_UNORM, nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  granit::texture_format_footprint cpp{};
  CHECK(granit::get_texture_format_footprint(granit::texture_format::bgra8_srgb, cpp) ==
        granit::result::success);
  CHECK(cpp.bytes_per_block == 4);
}

TEST_CASE("Texture Data Footprint处理压缩块和边缘Mip", "[texture][format][compressed]") {
  granit_texture_data_footprint native = GRANIT_TEXTURE_DATA_FOOTPRINT_INIT;
  REQUIRE(granit_texture_format_calculate_data_footprint(GRANIT_TEXTURE_FORMAT_BC1_RGBA_UNORM, 7, 5,
                                                         6, &native) == GRANIT_SUCCESS);
  CHECK(native.block_columns == 2);
  CHECK(native.block_rows == 2);
  CHECK(native.bytes_per_row == 16);
  CHECK(native.bytes_per_image == 32);
  CHECK(native.required_size == 192);

  granit::texture_data_footprint cpp{};
  REQUIRE(granit::calculate_texture_data_footprint(granit::texture_format::astc_4x4_srgb, 1, 1, 1,
                                                   cpp) == granit::result::success);
  CHECK(cpp.block_columns == 1);
  CHECK(cpp.block_rows == 1);
  CHECK(cpp.required_size == 16);
  CHECK(granit_texture_format_calculate_data_footprint(GRANIT_TEXTURE_FORMAT_BC7_RGBA_UNORM, 0, 4,
                                                       1,
                                                       &native) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Texture格式能力由当前设备查询", "[texture][format][capabilities]") {
  granit_texture_format_capabilities native = GRANIT_TEXTURE_FORMAT_CAPABILITIES_INIT;
  CHECK(granit_renderer_get_texture_format_capabilities(GRANIT_NULL_HANDLE,
                                                        GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                                                        &native) == GRANIT_ERROR_INVALID_HANDLE);

  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-texture-format-caps"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  REQUIRE(granit_renderer_get_texture_format_capabilities(renderer.native_handle(),
                                                          GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                                                          &native) == GRANIT_SUCCESS);
  CHECK(native.format == GRANIT_TEXTURE_FORMAT_RGBA8_UNORM);
  CHECK((native.supported_usage & GRANIT_TEXTURE_USAGE_SAMPLED_BIT) != 0);
  CHECK((native.supported_usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) != 0);

  granit::texture_format_capabilities cpp{};
  REQUIRE(granit::get_texture_format_capabilities(renderer.native_handle(),
                                                  granit::texture_format::bc1_rgba_unorm,
                                                  cpp) == granit::result::success);
  CHECK(cpp.format == granit::texture_format::bc1_rgba_unorm);
  if (cpp.supported_usage != granit::texture_usage{}) {
    CHECK(cpp.supports(granit::texture_usage::sampled));
    CHECK(cpp.supports(granit::texture_usage::transfer_destination));
    CHECK(cpp.filterable());
  }
}

TEST_CASE("压缩Texture写入遵守块对齐并允许边缘块", "[texture][write][compressed]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-compressed-upload"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit_texture_format_capabilities capabilities = GRANIT_TEXTURE_FORMAT_CAPABILITIES_INIT;
  REQUIRE(granit_renderer_get_texture_format_capabilities(renderer.native_handle(),
                                                          GRANIT_TEXTURE_FORMAT_BC1_RGBA_UNORM,
                                                          &capabilities) == GRANIT_SUCCESS);
  if ((capabilities.supported_usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0)
    SKIP("当前设备不支持 BC1 上传");

  granit::texture texture;
  REQUIRE(texture.initialize(renderer.native_handle(),
                             {.format = granit::texture_format::bc1_rgba_unorm,
                              .usage = granit::texture_usage::transfer_destination |
                                       granit::texture_usage::sampled,
                              .width = 7,
                              .height = 5}) == granit::result::success);
  std::array<std::byte, 32> blocks{};
  CHECK(texture.write(blocks, {}, {.width = 7, .height = 5}) == granit::result::success);
  CHECK(texture.write(blocks, {}, {.x = 1, .width = 4, .height = 4}) ==
        granit::result::invalid_argument);
  std::array<std::byte, 8> edge_block{};
  CHECK(texture.write(edge_block, {}, {.x = 4, .y = 4, .width = 3, .height = 1}) ==
        granit::result::success);
  granit::texture_readback_info info{};
  CHECK(texture.query_readback({.width = 7, .height = 5}, info) == granit::result::unsupported);
}

TEST_CASE("Texture同步读取先查询容量再返回紧密原始像素", "[texture][readback]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-texture-read"});
  if (unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  granit::texture texture;
  REQUIRE(texture.initialize(renderer.native_handle(),
                             {.format = granit::texture_format::rgba8_unorm,
                              .usage = granit::texture_usage::transfer_source |
                                       granit::texture_usage::transfer_destination,
                              .width = 2,
                              .height = 2}) == granit::result::success);
  constexpr std::array<uint8_t, 16> expected{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  REQUIRE(texture.write(std::as_bytes(std::span{expected}), {}, {.width = 2, .height = 2}) ==
          granit::result::success);
  granit::texture_readback_info info;
  REQUIRE(texture.query_readback({.width = 2, .height = 2}, info) == granit::result::success);
  CHECK(info.format == granit::texture_format::rgba8_unorm);
  CHECK(info.bytes_per_row == 8);
  CHECK(info.rows_per_image == 2);
  CHECK(info.required_size == expected.size());
  std::array<std::byte, 15> insufficient{};
  CHECK(texture.read(insufficient, {.width = 2, .height = 2}, info) ==
        granit::result::invalid_argument);
  std::array<std::byte, 16> actual{};
  REQUIRE(texture.read(actual, {.width = 2, .height = 2}, info) == granit::result::success);
  CHECK(std::memcmp(actual.data(), expected.data(), expected.size()) == 0);

  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-texture-read-second"}) ==
          granit::result::success);
  granit_texture_readback_info native_info = GRANIT_TEXTURE_READBACK_INFO_INIT;
  const granit_texture_write_region region{0, 0, 1, GRANIT_TEXTURE_ASPECT_COLOR_BIT, 0, 0, 0,
                                           2, 2, 1};
  uint64_t size = 0;
  CHECK(granit_texture_read(second.native_handle(), texture.native_handle(), &region, nullptr,
                            &size, &native_info) == GRANIT_ERROR_INVALID_HANDLE);
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

TEST_CASE("Cube Texture支持六面和Mip链", "[texture][cube][mip]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-cube-texture"});
  if (unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  granit::texture cube;
  REQUIRE(cube.initialize(renderer.native_handle(),
                          {.dimension = granit::texture_dimension::cube,
                           .format = granit::texture_format::rgba16_float,
                           .usage = granit::texture_usage::sampled |
                                    granit::texture_usage::transfer_destination,
                           .width = 8,
                           .height = 8,
                           .mip_levels = 4,
                           .array_layers = 6}) == granit::result::success);
  granit::texture_view view;
  REQUIRE(view.initialize(renderer.native_handle(), cube.native_handle(),
                          {.dimension = granit::texture_dimension::cube,
                           .format = granit::texture_format::rgba16_float,
                           .aspect = granit::texture_aspect::color,
                           .base_mip_level = 0,
                           .mip_level_count = 4,
                           .base_array_layer = 0,
                           .array_layer_count = 6}) == granit::result::success);

  std::array<std::byte, 2 * 2 * 8> pixels{};
  CHECK(cube.write(pixels, {},
                   {.mip_level = 2,
                    .base_array_layer = 5,
                    .array_layer_count = 1,
                    .width = 2,
                    .height = 2}) == granit::result::success);
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

TEST_CASE("Texture 写入校验用途、布局和区域", "[texture][write][validation]") {
  granit::renderer renderer;
  const auto result = renderer.initialize(
      {.application_name = "granit-texture-write-validation", .enable_validation = true});
  if (unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  granit::texture texture;
  REQUIRE(texture.initialize(renderer.native_handle(),
                             {.format = granit::texture_format::rgba8_unorm,
                              .usage = granit::texture_usage::transfer_destination,
                              .width = 4,
                              .height = 4}) == granit::result::success);
  std::array<std::byte, 64> pixels{};
  CHECK(texture.write(pixels, {}, {.width = 4, .height = 4}) == granit::result::success);
  CHECK(texture.write(pixels, {.bytes_per_row = 15}, {.width = 4, .height = 4}) ==
        granit::result::invalid_argument);
  CHECK(texture.write(pixels, {}, {.x = 1, .width = 4, .height = 4}) ==
        granit::result::invalid_argument);

  granit::texture sampled;
  REQUIRE(
      sampled.initialize(renderer.native_handle(), {.format = granit::texture_format::rgba8_unorm,
                                                    .usage = granit::texture_usage::sampled,
                                                    .width = 4,
                                                    .height = 4}) == granit::result::success);
  CHECK(sampled.write(pixels, {}, {.width = 4, .height = 4}) == granit::result::unsupported);
}

TEST_CASE("不同 Texture 可以并发写入", "[texture][write][concurrency]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-texture-write-concurrency"});
  if (unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  constexpr std::size_t count = 8;
  std::array<granit::texture, count> textures;
  for (auto& texture : textures) {
    REQUIRE(texture.initialize(renderer.native_handle(),
                               {.format = granit::texture_format::rgba8_unorm,
                                .usage = granit::texture_usage::transfer_destination,
                                .width = 16,
                                .height = 16}) == granit::result::success);
  }
  std::array<std::byte, 16 * 16 * 4> pixels{};
  std::vector<std::future<granit::result>> workers;
  workers.reserve(count);
  for (auto& texture : textures) {
    workers.push_back(std::async(std::launch::async, [&texture, &pixels] {
      return texture.write(pixels, {}, {.width = 16, .height = 16});
    }));
  }
  for (auto& worker : workers)
    CHECK(worker.get() == granit::result::success);
}
} // namespace
