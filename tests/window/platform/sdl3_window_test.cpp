// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>
#include <granit/window/native.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstdint>

namespace {

SDL_WindowID find_window_id(int expected_width) {
  int count = 0;
  auto** windows = SDL_GetWindows(&count);
  for (int index = 0; index < count; ++index) {
    int width = 0;
    int height = 0;
    if (SDL_GetWindowSize(windows[index], &width, &height) && width == expected_width)
      return SDL_GetWindowID(windows[index]);
  }
  return 0;
}

} // namespace

TEST_CASE("SDL3 Window 后端统一管理多窗口和输入路由", "[window][sdl3]") {
  granit_window_system_desc system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  system_desc.backend = GRANIT_WINDOW_BACKEND_SDL3;
  granit_window_system system = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_system_create(&system_desc, &system) == GRANIT_SUCCESS);

  granit_window_system duplicate = UINT64_C(42);
  CHECK(granit_window_system_create(&system_desc, &duplicate) == GRANIT_ERROR_RESOURCE_IN_USE);
  CHECK(duplicate == GRANIT_NULL_HANDLE);

  std::array<granit_window, 2> windows{};
  for (std::size_t index = 0; index < windows.size(); ++index) {
    granit_window_desc desc = GRANIT_WINDOW_DESC_INIT;
    desc.title = "Granit SDL3 Window Test";
    desc.title_length = 23;
    desc.width = index == 0 ? 320 : 640;
    desc.height = 240;
    REQUIRE(granit_window_create(system, &desc, &windows[index]) == GRANIT_SUCCESS);
  }

  const auto first_id = find_window_id(320);
  const auto second_id = find_window_id(640);
  REQUIRE(first_id != 0);
  REQUIRE(second_id != 0);
  REQUIRE(first_id != second_id);

  // SDL 可能在创建窗口后异步产生鼠标进入等初始事件，先清空再验证注入顺序。
  REQUIRE(granit_window_system_process_events(system) == GRANIT_SUCCESS);
  granit_input_event input = GRANIT_INPUT_EVENT_INIT;
  while (granit_window_poll_input_event(system, &input) == GRANIT_SUCCESS)
    input = GRANIT_INPUT_EVENT_INIT;
  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  while (granit_window_poll_event(system, &event) == GRANIT_SUCCESS)
    event = GRANIT_WINDOW_EVENT_INIT;

  SDL_Event motion{};
  motion.type = SDL_EVENT_MOUSE_MOTION;
  motion.motion.windowID = second_id;
  motion.motion.x = 25.5F;
  motion.motion.y = 12.25F;
  motion.motion.xrel = 3.0F;
  motion.motion.yrel = -2.0F;
  REQUIRE(SDL_PushEvent(&motion));

  SDL_Event key{};
  key.type = SDL_EVENT_KEY_DOWN;
  key.key.windowID = first_id;
  key.key.scancode = SDL_SCANCODE_F;
  REQUIRE(SDL_PushEvent(&key));

  REQUIRE(granit_window_system_process_events(system) == GRANIT_SUCCESS);
  input = GRANIT_INPUT_EVENT_INIT;
  REQUIRE(granit_window_poll_input_event(system, &input) == GRANIT_SUCCESS);
  CHECK(input.window == windows[1]);
  CHECK(input.type == GRANIT_INPUT_EVENT_POINTER_MOVED);
  CHECK(input.data.pointer_moved.x == Catch::Approx(25.5F));
  REQUIRE(granit_window_poll_input_event(system, &input) == GRANIT_SUCCESS);
  CHECK(input.window == windows[0]);
  CHECK(input.type == GRANIT_INPUT_EVENT_KEY);
  CHECK(input.data.key.physical_key == GRANIT_PHYSICAL_KEY_F);

  granit_window_native_win32 native = GRANIT_WINDOW_NATIVE_WIN32_INIT;
  CHECK(granit_window_get_native_win32(system, windows[0], &native) == GRANIT_ERROR_UNSUPPORTED);
  CHECK(native.window == nullptr);

  for (const auto id : {first_id, second_id}) {
    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    close.window.windowID = id;
    REQUIRE(SDL_PushEvent(&close));
  }
  REQUIRE(granit_window_system_process_events(system) == GRANIT_SUCCESS);
  std::array<bool, 2> saw_close{};
  event = GRANIT_WINDOW_EVENT_INIT;
  while (granit_window_poll_event(system, &event) == GRANIT_SUCCESS) {
    if (event.type == GRANIT_WINDOW_EVENT_CLOSE_REQUESTED) {
      if (event.window == windows[0])
        saw_close[0] = true;
      if (event.window == windows[1])
        saw_close[1] = true;
    }
    event = GRANIT_WINDOW_EVENT_INIT;
  }
  CHECK(saw_close[0]);
  CHECK(saw_close[1]);

  REQUIRE(granit_window_destroy(system, windows[0]) == GRANIT_SUCCESS);
  REQUIRE(granit_window_destroy(system, windows[1]) == GRANIT_SUCCESS);
  REQUIRE(granit_window_system_destroy(system) == GRANIT_SUCCESS);
}
