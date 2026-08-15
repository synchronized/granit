// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/timestamp_query.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

TEST_CASE("Command Recorder 强制执行空录制状态机", "[command][recorder]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-command-tests"});
  if (environment_unavailable(result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == granit::result::success);

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  CHECK(recorder.end() == granit::result::invalid_argument);
  CHECK(recorder.reset() == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  CHECK(recorder.begin() == granit::result::invalid_argument);
  CHECK(recorder.reset() == granit::result::invalid_argument);
  REQUIRE(recorder.end() == granit::result::success);
  CHECK(recorder.end() == granit::result::invalid_argument);
  REQUIRE(recorder.reset() == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);

  granit::command_recorder moved{std::move(recorder)};
  CHECK_FALSE(recorder.valid());
  CHECK(moved.valid());
  CHECK(moved.destroy() == granit::result::success);
}

TEST_CASE("Recorder 异步提交并按 frames-in-flight 复用槽位", "[command][submit]") {
  granit::renderer renderer;
  const auto result =
      renderer.initialize({.application_name = "granit-submit-tests", .frames_in_flight = 2});
  if (environment_unavailable(result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == granit::result::success);

  std::vector<granit::command_recorder> recorders(3);
  for (auto& recorder : recorders) {
    REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
    CHECK(recorder.submit() == granit::result::invalid_argument);
    REQUIRE(recorder.begin() == granit::result::success);
    REQUIRE(recorder.end() == granit::result::success);
    REQUIRE(recorder.submit() == granit::result::success);
    CHECK(recorder.submit() == granit::result::invalid_argument);
  }

  for (auto& recorder : recorders) {
    REQUIRE(recorder.reset() == granit::result::success);
    REQUIRE(recorder.begin() == granit::result::success);
    REQUIRE(recorder.end() == granit::result::success);
  }
}

TEST_CASE("Recorder 批量提交共享一次完成边界", "[command][submit][batch]") {
  granit::renderer renderer;
  const auto result =
      renderer.initialize({.application_name = "granit-batch-submit-tests", .frames_in_flight = 2});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  std::array<granit::command_recorder, 3> recorders;
  for (auto& recorder : recorders) {
    REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
    REQUIRE(recorder.begin() == granit::result::success);
    REQUIRE(recorder.end() == granit::result::success);
  }
  REQUIRE(granit::command_recorder::submit_batch(recorders) == granit::result::success);
  for (auto& recorder : recorders)
    CHECK(recorder.submit() == granit::result::invalid_argument);
  for (auto& recorder : recorders)
    REQUIRE(recorder.reset() == granit::result::success);
}

TEST_CASE("Recorder 批量提交失败时不产生部分提交", "[command][submit][batch][validation]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-batch-validation-tests"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  std::array<granit::command_recorder, 2> recorders;
  for (auto& recorder : recorders) {
    REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
    REQUIRE(recorder.begin() == granit::result::success);
    REQUIRE(recorder.end() == granit::result::success);
  }
  const granit_command_recorder duplicate[]{recorders[0].native_handle(),
                                            recorders[0].native_handle()};
  CHECK(granit_command_recorder_submit_batch(renderer.native_handle(), duplicate, 2) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  const granit_command_recorder invalid[]{recorders[0].native_handle(), UINT64_C(1)};
  CHECK(granit_command_recorder_submit_batch(renderer.native_handle(), invalid, 2) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_command_recorder_submit_batch(renderer.native_handle(), nullptr, 2) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_command_recorder_submit_batch(renderer.native_handle(), invalid, 0) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  REQUIRE(recorders[0].submit() == granit::result::success);
  REQUIRE(recorders[1].submit() == granit::result::success);
}

TEST_CASE("Command Recorder 校验 Renderer domain 和失效句柄", "[command][handle]") {
  granit::renderer first;
  const auto result = first.initialize({.application_name = "granit-command-first"});
  if (environment_unavailable(result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == granit::result::success);
  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-command-second"}) ==
          granit::result::success);

  granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  granit_command_recorder recorder = GRANIT_NULL_HANDLE;
  REQUIRE(granit_command_recorder_create(first.native_handle(), &desc, &recorder) ==
          GRANIT_SUCCESS);
  CHECK(granit_command_recorder_begin(second.native_handle(), recorder) ==
        GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_command_recorder_destroy(first.native_handle(), recorder) == GRANIT_SUCCESS);
  CHECK(granit_command_recorder_destroy(first.native_handle(), recorder) ==
        GRANIT_ERROR_INVALID_HANDLE);

  desc.flags = UINT32_C(1);
  CHECK(granit_command_recorder_create(first.native_handle(), &desc, &recorder) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(recorder == GRANIT_NULL_HANDLE);
}

TEST_CASE("Renderer 销毁会使所属 Command Recorder 失效", "[command][lifetime]") {
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const auto result = granit_renderer_create(&renderer_desc, &renderer);
  if (environment_unavailable(granit::from_native(result))) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == GRANIT_SUCCESS);
  granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  granit_command_recorder recorder = GRANIT_NULL_HANDLE;
  REQUIRE(granit_command_recorder_create(renderer, &recorder_desc, &recorder) == GRANIT_SUCCESS);

  REQUIRE(granit_renderer_destroy(renderer) == GRANIT_SUCCESS);
  CHECK(granit_command_recorder_destroy(renderer, recorder) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Recorder 批量复制和填充 Buffer 并保留内部资源", "[command][copy]") {
  granit::renderer renderer;
  const auto result = renderer.initialize(
      {.application_name = "granit-copy-command-tests", .enable_validation = true});
  if (result == granit::result::unsupported)
    SKIP("当前运行环境没有 Khronos validation layer");
  if (environment_unavailable(result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);

  granit_buffer_desc source_desc = GRANIT_BUFFER_DESC_INIT;
  source_desc.size = 64;
  source_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT;
  granit_buffer source = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer.native_handle(), &source_desc, &source) == GRANIT_SUCCESS);
  granit_buffer_desc destination_desc = GRANIT_BUFFER_DESC_INIT;
  destination_desc.size = 64;
  destination_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  granit_buffer destination = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer.native_handle(), &destination_desc, &destination) ==
          GRANIT_SUCCESS);

  constexpr granit::buffer_copy_region regions[]{
      {.source_offset = 0, .destination_offset = 16, .size = 16},
      {.source_offset = 32, .destination_offset = 48, .size = 16}};
  CHECK(recorder.copy_buffer(source, destination, regions) == granit::result::invalid_argument);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.copy_buffer(source, destination, regions) == granit::result::success);
  REQUIRE(recorder.fill_buffer(destination, 0, 16, UINT32_C(0xff00ff00)) ==
          granit::result::success);

  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(granit_buffer_destroy(renderer.native_handle(), source) == GRANIT_SUCCESS);
  REQUIRE(granit_buffer_destroy(renderer.native_handle(), destination) == GRANIT_SUCCESS);
  REQUIRE(recorder.reset() == granit::result::success);
  CHECK(granit_buffer_destroy(renderer.native_handle(), source) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Buffer 命令拒绝 usage、范围、对齐和重叠错误", "[command][copy][validation]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-copy-validation-tests"});
  if (environment_unavailable(result)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(result == granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);

  granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
  desc.size = 64;
  desc.usage =
      GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT | GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  REQUIRE(granit_buffer_create(renderer.native_handle(), &desc, &buffer) == GRANIT_SUCCESS);
  const granit::buffer_copy_region overlap{.source_offset = 0, .destination_offset = 8, .size = 16};
  CHECK(recorder.copy_buffer(buffer, buffer, std::span{&overlap, 1}) ==
        granit::result::invalid_argument);
  const granit::buffer_copy_region out_of_range{
      .source_offset = 60, .destination_offset = 0, .size = 8};
  CHECK(recorder.copy_buffer(buffer, buffer, std::span{&out_of_range, 1}) ==
        granit::result::invalid_argument);
  CHECK(recorder.fill_buffer(buffer, 2, 16, 0) == granit::result::invalid_argument);
  CHECK(recorder.fill_buffer(buffer, 0, 6, 0) == granit::result::invalid_argument);
  const granit::buffer_copy_region non_overlapping{
      .source_offset = 0, .destination_offset = 32, .size = 16};
  CHECK(recorder.copy_buffer(buffer, buffer, std::span{&non_overlapping, 1}) ==
        granit::result::success);

  REQUIRE(recorder.end() == granit::result::success);
}

TEST_CASE("Recorder 将 Texture 复制到 Readback Buffer", "[command][copy][texture]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-texture-readback-tests"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage =
      GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
  texture_desc.width = 2;
  texture_desc.height = 2;
  granit_texture texture = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create(renderer.native_handle(), &texture_desc, &texture) ==
          GRANIT_SUCCESS);
  constexpr std::array<std::uint8_t, 16> pixels{1, 2,  3,  4,  5,  6,  7,  8,
                                                9, 10, 11, 12, 13, 14, 15, 16};
  const granit_texture_data_layout layout{};
  const granit_texture_write_region region{.mip_level = 0,
                                           .base_array_layer = 0,
                                           .array_layer_count = 1,
                                           .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                           .x = 0,
                                           .y = 0,
                                           .z = 0,
                                           .width = 2,
                                           .height = 2,
                                           .depth = 1};
  REQUIRE(granit_texture_write(renderer.native_handle(), texture, pixels.data(), pixels.size(),
                               &layout, &region) == GRANIT_SUCCESS);

  granit::buffer readback;
  REQUIRE(readback.initialize(renderer.native_handle(),
                              {.size = pixels.size(),
                               .usage = granit::buffer_usage::transfer_destination,
                               .location = granit::memory_location::readback}) ==
          granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  CHECK(recorder.copy_texture_to_buffer(texture, readback.native_handle(), layout, region) ==
        granit::result::invalid_argument);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.copy_texture_to_buffer(texture, readback.native_handle(), layout, region) ==
          granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  void* mapped = nullptr;
  REQUIRE(readback.map(0, pixels.size(), &mapped) == granit::result::success);
  CHECK(std::memcmp(mapped, pixels.data(), pixels.size()) == 0);
  REQUIRE(readback.unmap() == granit::result::success);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
}

TEST_CASE("Recorder 在 Texture 之间复制区域", "[command][copy][texture]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-texture-copy-tests"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  granit::texture source;
  granit::texture destination;
  const granit::texture_desc source_desc{.format = granit::texture_format::rgba8_unorm,
                                         .usage = granit::texture_usage::transfer_source |
                                                  granit::texture_usage::transfer_destination,
                                         .width = 2,
                                         .height = 2};
  const granit::texture_desc destination_desc{.format = granit::texture_format::rgba8_unorm,
                                              .usage = granit::texture_usage::transfer_source |
                                                       granit::texture_usage::transfer_destination,
                                              .width = 2,
                                              .height = 2};
  REQUIRE(source.initialize(renderer.native_handle(), source_desc) == granit::result::success);
  REQUIRE(destination.initialize(renderer.native_handle(), destination_desc) ==
          granit::result::success);
  constexpr std::array<std::uint8_t, 16> pixels{1, 2,  3,  4,  5,  6,  7,  8,
                                                9, 10, 11, 12, 13, 14, 15, 16};
  const granit::texture_data_layout layout{};
  const granit::texture_write_region write_region{.width = 2, .height = 2};
  REQUIRE(source.write(std::as_bytes(std::span{pixels}), layout, write_region) ==
          granit::result::success);

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  const granit::texture_copy_region copy_region{.array_layer_count = 1,
                                                .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                .width = 2,
                                                .height = 2,
                                                .depth = 1};
  CHECK(recorder.copy_texture(source.native_handle(), destination.native_handle(), copy_region) ==
        granit::result::invalid_argument);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.copy_texture(source.native_handle(), destination.native_handle(), copy_region) ==
          granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  granit::texture_readback_info info;
  REQUIRE(destination.query_readback(write_region, info) == granit::result::success);
  std::vector<std::byte> copied(static_cast<std::size_t>(info.required_size));
  REQUIRE(destination.read(copied, write_region, info) == granit::result::success);
  CHECK(std::memcmp(copied.data(), pixels.data(), pixels.size()) == 0);
}

TEST_CASE("Recorder 将 Upload Buffer 复制到 Texture", "[command][copy][texture]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-buffer-texture-tests"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  constexpr std::array<std::uint8_t, 16> pixels{21, 22, 23, 24, 25, 26, 27, 28,
                                                29, 30, 31, 32, 33, 34, 35, 36};
  granit::buffer upload;
  REQUIRE(upload.initialize(renderer.native_handle(),
                            {.size = pixels.size(),
                             .usage = granit::buffer_usage::transfer_source,
                             .location = granit::memory_location::upload},
                            std::as_bytes(std::span{pixels})) == granit::result::success);
  granit::texture destination;
  REQUIRE(destination.initialize(renderer.native_handle(),
                                 {.format = granit::texture_format::rgba8_unorm,
                                  .usage = granit::texture_usage::transfer_source |
                                           granit::texture_usage::transfer_destination,
                                  .width = 2,
                                  .height = 2}) == granit::result::success);
  const granit_texture_data_layout layout{};
  const granit_texture_write_region region{.array_layer_count = 1,
                                           .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                           .width = 2,
                                           .height = 2,
                                           .depth = 1};

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  CHECK(recorder.copy_buffer_to_texture(upload.native_handle(), destination.native_handle(), layout,
                                        region) == granit::result::invalid_argument);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.copy_buffer_to_texture(upload.native_handle(), destination.native_handle(),
                                          layout, region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  const granit::texture_write_region read_region{.width = 2, .height = 2};
  granit::texture_readback_info info;
  REQUIRE(destination.query_readback(read_region, info) == granit::result::success);
  std::vector<std::byte> copied(static_cast<std::size_t>(info.required_size));
  REQUIRE(destination.read(copied, read_region, info) == granit::result::success);
  CHECK(std::memcmp(copied.data(), pixels.data(), pixels.size()) == 0);
}

TEST_CASE("Recorder 独立跟踪同一 Texture 的不同 mip", "[command][state][texture]") {
  granit::renderer renderer;
  const auto result = renderer.initialize(
      {.application_name = "granit-mip-state-tests", .enable_validation = true});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  granit::texture texture;
  REQUIRE(texture.initialize(renderer.native_handle(),
                             {.format = granit::texture_format::rgba8_unorm,
                              .usage = granit::texture_usage::transfer_source |
                                       granit::texture_usage::transfer_destination,
                              .width = 2,
                              .height = 2,
                              .mip_levels = 2}) == granit::result::success);
  constexpr std::array<std::uint8_t, 16> mip_zero{1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
  constexpr std::array<std::uint8_t, 4> mip_one{9, 10, 11, 12};
  REQUIRE(texture.write(std::as_bytes(std::span{mip_zero}), {}, {.width = 2, .height = 2}) ==
          granit::result::success);
  REQUIRE(texture.write(std::as_bytes(std::span{mip_one}), {},
                        {.mip_level = 1, .width = 1, .height = 1}) == granit::result::success);

  granit::buffer readback;
  REQUIRE(readback.initialize(renderer.native_handle(),
                              {.size = mip_zero.size() + mip_one.size(),
                               .usage = granit::buffer_usage::transfer_destination,
                               .location = granit::memory_location::readback}) ==
          granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit_texture_write_region mip_zero_region{.array_layer_count = 1,
                                                    .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                    .width = 2,
                                                    .height = 2,
                                                    .depth = 1};
  const granit_texture_data_layout mip_one_layout{.offset = mip_zero.size()};
  const granit_texture_write_region mip_one_region{.mip_level = 1,
                                                   .array_layer_count = 1,
                                                   .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                   .width = 1,
                                                   .height = 1,
                                                   .depth = 1};
  REQUIRE(recorder.copy_texture_to_buffer(texture.native_handle(), readback.native_handle(), {},
                                          mip_zero_region) == granit::result::success);
  REQUIRE(recorder.copy_texture_to_buffer(texture.native_handle(), readback.native_handle(),
                                          mip_one_layout,
                                          mip_one_region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  void* mapped = nullptr;
  REQUIRE(readback.map(0, mip_zero.size() + mip_one.size(), &mapped) == granit::result::success);
  CHECK(std::memcmp(mapped, mip_zero.data(), mip_zero.size()) == 0);
  CHECK(std::memcmp(static_cast<std::byte*>(mapped) + mip_zero.size(), mip_one.data(),
                    mip_one.size()) == 0);
  REQUIRE(readback.unmap() == granit::result::success);
}

TEST_CASE("Recorder 录制 Dynamic Rendering 作用域", "[command][rendering]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-rendering-tests"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);
  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.width = 32;
  texture_desc.height = 32;
  granit_texture texture{};
  granit_texture_view view{};
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &texture_desc, &texture,
                                                  &view) == GRANIT_SUCCESS);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit::color_attachment_desc color{.view = view};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {.width = 32, .height = 32}};
  REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
  CHECK(recorder.begin_rendering(rendering) == granit::result::invalid_argument);
  CHECK(recorder.end() == granit::result::invalid_argument);
  REQUIRE(recorder.end_rendering() == granit::result::success);
  CHECK(recorder.end_rendering() == granit::result::invalid_argument);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
}

TEST_CASE("Texture Layout 按提交顺序解析而非录制顺序", "[command][rendering][layout]") {
  granit::renderer renderer;
  const auto result =
      renderer.initialize({.application_name = "granit-layout-tests", .enable_validation = true});
  if (result == granit::result::unsupported)
    SKIP("当前运行环境没有 Khronos validation layer");
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);
  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.width = 16;
  texture_desc.height = 16;
  granit_texture texture{};
  granit_texture_view view{};
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &texture_desc, &texture,
                                                  &view) == GRANIT_SUCCESS);

  std::array<granit::command_recorder, 2> recorders;
  auto& load_recorder = recorders[0];
  auto& clear_recorder = recorders[1];
  REQUIRE(load_recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(clear_recorder.initialize(renderer.native_handle()) == granit::result::success);
  const granit::color_attachment_desc load_color{
      .view = view, .load_operation = granit::attachment_load_operation::load};
  const granit::color_attachment_desc clear_color{.view = view};
  const auto record = [&](granit::command_recorder& recorder,
                          const granit::color_attachment_desc& color) {
    REQUIRE(recorder.begin() == granit::result::success);
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                           .area = {.width = 16, .height = 16}};
    REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
    REQUIRE(recorder.end_rendering() == granit::result::success);
    REQUIRE(recorder.end() == granit::result::success);
  };

  record(load_recorder, load_color);
  record(clear_recorder, clear_color);
  CHECK(granit::command_recorder::submit_batch(recorders) == granit::result::invalid_argument);
  const granit_command_recorder clear_then_load[]{clear_recorder.native_handle(),
                                                  load_recorder.native_handle()};
  REQUIRE(granit_command_recorder_submit_batch(renderer.native_handle(), clear_then_load, 2) ==
          GRANIT_SUCCESS);
  REQUIRE(clear_recorder.reset() == granit::result::success);
  REQUIRE(load_recorder.reset() == granit::result::success);
}

TEST_CASE("独立 Recorder 支持并行资源上传与命令录制", "[command][concurrency][upload]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-parallel-recording"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  constexpr std::size_t worker_count = 8;
  constexpr std::uint64_t buffer_size = 256;
  const std::array<std::byte, buffer_size> initial_data{};
  std::array<granit::buffer, worker_count> buffers;
  std::array<granit::command_recorder, worker_count> recorders;
  std::barrier start{worker_count};
  std::atomic_uint32_t failures{};
  std::vector<std::thread> workers;
  workers.reserve(worker_count);
  for (std::size_t index = 0; index < worker_count; ++index) {
    workers.emplace_back([&, index] {
      start.arrive_and_wait();
      auto worker_result =
          buffers[index].initialize(renderer.native_handle(),
                                    {.size = buffer_size,
                                     .usage = granit::buffer_usage::transfer_source |
                                              granit::buffer_usage::transfer_destination,
                                     .location = granit::memory_location::device},
                                    initial_data);
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].initialize(renderer.native_handle());
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].begin();
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].fill_buffer(buffers[index].native_handle(), 0, buffer_size,
                                                     static_cast<std::uint32_t>(index));
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].end();
      if (granit::failed(worker_result))
        ++failures;
    });
  }
  for (auto& worker : workers)
    worker.join();
  REQUIRE(failures.load() == 0);
  for (auto& recorder : recorders) {
    REQUIRE(recorder.submit() == granit::result::success);
    REQUIRE(recorder.reset() == granit::result::success);
  }
}

TEST_CASE("独立 Texture 支持并行创建与颜色附件录制", "[command][concurrency][texture]") {
  granit::renderer renderer;
  const auto result = renderer.initialize(
      {.application_name = "granit-parallel-textures", .enable_validation = true});
  if (result == granit::result::unsupported)
    SKIP("当前运行环境没有 Khronos validation layer");
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  constexpr std::size_t worker_count = 8;
  std::array<granit_texture, worker_count> textures{};
  std::array<granit_texture_view, worker_count> views{};
  std::array<granit::command_recorder, worker_count> recorders;
  std::barrier start{worker_count};
  std::atomic_uint32_t failures{};
  std::vector<std::thread> workers;
  workers.reserve(worker_count);
  for (std::size_t index = 0; index < worker_count; ++index) {
    workers.emplace_back([&, index] {
      start.arrive_and_wait();
      granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
      desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
      desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
      desc.width = 32;
      desc.height = 32;
      auto worker_result = granit::from_native(granit_texture_create_with_default_view(
          renderer.native_handle(), &desc, &textures[index], &views[index]));
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].initialize(renderer.native_handle());
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].begin();
      const granit::color_attachment_desc color{
          .view = views[index],
          .clear_value = {.red = static_cast<float>(index) / static_cast<float>(worker_count),
                          .green = 0.2F,
                          .blue = 0.4F,
                          .alpha = 1.0F}};
      const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                             .area = {.width = 32, .height = 32}};
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].begin_rendering(rendering);
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].end_rendering();
      if (granit::succeeded(worker_result))
        worker_result = recorders[index].end();
      if (granit::failed(worker_result))
        ++failures;
    });
  }
  for (auto& worker : workers)
    worker.join();
  REQUIRE(failures.load() == 0);
  for (auto& recorder : recorders) {
    REQUIRE(recorder.submit() == granit::result::success);
    REQUIRE(recorder.reset() == granit::result::success);
  }
  for (std::size_t index = 0; index < worker_count; ++index) {
    REQUIRE(granit_texture_view_destroy(renderer.native_handle(), views[index]) == GRANIT_SUCCESS);
    REQUIRE(granit_texture_destroy(renderer.native_handle(), textures[index]) == GRANIT_SUCCESS);
  }
}

TEST_CASE("Command Recorder 写入并读取GPU纳秒时间戳", "[command][timestamp]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-timestamp-command"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::timestamp_query_pool queries;
  granit::command_recorder recorder;
  REQUIRE(queries.initialize(renderer.native_handle(), 2) == granit::result::success);
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.reset_timestamp_queries(queries.native_handle(), 0, 2) ==
          granit::result::success);
  REQUIRE(recorder.write_timestamp(queries.native_handle(), GRANIT_TIMESTAMP_STAGE_TOP, 0) ==
          granit::result::success);
  REQUIRE(recorder.write_timestamp(queries.native_handle(), GRANIT_TIMESTAMP_STAGE_BOTTOM, 1) ==
          granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  std::array<std::uint64_t, 2> nanoseconds{};
  REQUIRE(queries.get_results(0, nanoseconds) == granit::result::success);
  CHECK(nanoseconds[1] >= nanoseconds[0]);
  REQUIRE(recorder.destroy() == granit::result::success);
  REQUIRE(queries.reset() == granit::result::success);
}

} // namespace
