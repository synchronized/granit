// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/buffer.h>
#include <granit/command_recorder.hpp>
#include <granit/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <utility>

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
  const auto result = renderer.initialize({.application_name = "granit-copy-command-tests"});
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

  REQUIRE(granit_buffer_destroy(renderer.native_handle(), source) == GRANIT_SUCCESS);
  REQUIRE(granit_buffer_destroy(renderer.native_handle(), destination) == GRANIT_SUCCESS);
  REQUIRE(recorder.end() == granit::result::success);
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

  REQUIRE(recorder.end() == granit::result::success);
}

} // namespace
