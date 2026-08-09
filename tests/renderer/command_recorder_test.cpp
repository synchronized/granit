// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

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

} // namespace
