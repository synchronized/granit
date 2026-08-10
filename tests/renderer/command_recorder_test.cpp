// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.h>

#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
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

  granit::command_recorder load_recorder;
  granit::command_recorder clear_recorder;
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
  CHECK(load_recorder.submit() == granit::result::invalid_argument);
  REQUIRE(clear_recorder.submit() == granit::result::success);
  REQUIRE(load_recorder.submit() == granit::result::success);
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

} // namespace
