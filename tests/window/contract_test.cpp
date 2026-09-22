// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>
#include <granit/window.hpp>
#include <granit/window/native.h>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <thread>

namespace {

template <typename T, std::size_t VersionSize>
void check_versioned_output(granit_result (*call)(T*), granit_result expected) {
  alignas(T) std::array<std::byte, VersionSize + 8> storage{};
  std::memset(storage.data(), 0x5a, storage.size());
  auto* output = reinterpret_cast<T*>(storage.data());
  const auto capacity = static_cast<uint32_t>(VersionSize);
  std::memcpy(&output->struct_size, &capacity, sizeof(capacity));
  CHECK(call(output) == expected);
  CHECK(output->struct_size == VersionSize);
  for (std::size_t index = VersionSize; index < storage.size(); ++index)
    CHECK(storage[index] == std::byte{0x5a});
}

struct loop_test_application {
  std::uint32_t ticks{};
  std::uint32_t shutdowns{};
  granit::result shutdown_reason{granit::result::unknown};
  granit_window_system system{};
  granit::result destroy_result{granit::result::unknown};

  granit::result tick(granit::window_loop_action& action) noexcept {
    ++ticks;
    if (ticks == 1)
      destroy_result = granit::from_native(granit_window_system_destroy(system));
    action = ticks >= 3 ? granit::window_loop_action::stop
                        : granit::window_loop_action::continue_running;
    return granit::result::success;
  }

  void shutdown(granit::result reason) noexcept {
    ++shutdowns;
    shutdown_reason = reason;
  }
};

struct failing_loop_application {
  std::uint32_t shutdowns{};
  granit::result shutdown_reason{granit::result::unknown};

  granit::result tick(granit::window_loop_action&) noexcept { return granit::result::cancelled; }

  void shutdown(granit::result reason) noexcept {
    ++shutdowns;
    shutdown_reason = reason;
  }
};

struct raw_loop_context {
  granit_window_system system{};
  const granit_window_loop_desc* desc{};
  granit_window_loop_action action{GRANIT_WINDOW_LOOP_STOP};
  granit_result nested_result{GRANIT_ERROR_UNKNOWN};
  granit_result shutdown_reason{GRANIT_ERROR_UNKNOWN};
  std::uint32_t shutdowns{};
};

granit_result GRANIT_WINDOW_CALLBACK raw_loop_tick(void* user_data,
                                                   granit_window_loop_action* action) {
  auto& context = *static_cast<raw_loop_context*>(user_data);
  if (context.desc != nullptr)
    context.nested_result = granit_window_system_run_loop(context.system, context.desc);
  *action = context.action;
  return GRANIT_SUCCESS;
}

void GRANIT_WINDOW_CALLBACK raw_loop_shutdown(void* user_data, granit_result reason) {
  auto& context = *static_cast<raw_loop_context*>(user_data);
  ++context.shutdowns;
  context.shutdown_reason = reason;
}

} // namespace

TEST_CASE("Window Loop 在同一线程推进并且只关闭一次", "[window][loop]") {
  granit::window_system system;
  const auto initialize_result = system.initialize();
  if (initialize_result == granit::result::backend_unavailable)
    SKIP("当前环境没有可用的 Window 后端");
  REQUIRE(initialize_result == granit::result::success);

  loop_test_application application{.system = system.native_handle()};
  CHECK(granit::run_window_loop(system, application) == granit::result::success);
  CHECK(application.ticks == 3);
  CHECK(application.shutdowns == 1);
  CHECK(application.shutdown_reason == granit::result::success);
  CHECK(application.destroy_result == granit::result::resource_in_use);

  failing_loop_application failing;
  CHECK(granit::run_window_loop(system, failing) == granit::result::cancelled);
  CHECK(failing.shutdowns == 1);
  CHECK(failing.shutdown_reason == granit::result::cancelled);
}

TEST_CASE("Window Loop 在接受回调前校验描述", "[window][loop][abi]") {
  granit_window_loop_desc desc = GRANIT_WINDOW_LOOP_DESC_INIT;
  CHECK(granit_window_system_run_loop(UINT64_MAX, nullptr) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_window_system_run_loop(UINT64_MAX, &desc) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Window 原生快照在访问句柄前校验输出容量", "[window][native][abi]") {
  granit_window_native_win32 win32 = GRANIT_WINDOW_NATIVE_WIN32_INIT;
  win32.struct_size = GRANIT_WINDOW_NATIVE_WIN32_VERSION_1_SIZE - 1;
  CHECK(granit_window_get_native_win32(UINT64_MAX, UINT64_MAX, &win32) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_window_get_native_win32(UINT64_MAX, UINT64_MAX, nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  granit_window_native_xcb xcb = GRANIT_WINDOW_NATIVE_XCB_INIT;
  xcb.struct_size = GRANIT_WINDOW_NATIVE_XCB_VERSION_1_SIZE - 1;
  CHECK(granit_window_get_native_xcb(UINT64_MAX, UINT64_MAX, &xcb) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  granit_window_native_wayland wayland = GRANIT_WINDOW_NATIVE_WAYLAND_INIT;
  wayland.struct_size = GRANIT_WINDOW_NATIVE_WAYLAND_VERSION_1_SIZE - 1;
  CHECK(granit_window_get_native_wayland(UINT64_MAX, UINT64_MAX, &wayland) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  granit_window_native_emscripten emscripten = GRANIT_WINDOW_NATIVE_EMSCRIPTEN_INIT;
  emscripten.struct_size = GRANIT_WINDOW_NATIVE_EMSCRIPTEN_VERSION_1_SIZE - 1;
  CHECK(granit_window_get_native_emscripten(UINT64_MAX, UINT64_MAX, &emscripten) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Window Loop 拒绝未知动作和递归运行", "[window][loop]") {
  granit::window_system system;
  const auto initialize_result = system.initialize();
  if (initialize_result == granit::result::backend_unavailable)
    SKIP("当前环境没有可用的 Window 后端");
  REQUIRE(initialize_result == granit::result::success);

  raw_loop_context unknown;
  unknown.system = system.native_handle();
  unknown.action = UINT32_MAX;
  granit_window_loop_desc unknown_desc = GRANIT_WINDOW_LOOP_DESC_INIT;
  unknown_desc.tick = raw_loop_tick;
  unknown_desc.shutdown = raw_loop_shutdown;
  unknown_desc.user_data = &unknown;
  CHECK(granit_window_system_run_loop(unknown.system, &unknown_desc) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(unknown.shutdowns == 1);
  CHECK(unknown.shutdown_reason == GRANIT_ERROR_INVALID_ARGUMENT);

  raw_loop_context recursive;
  recursive.system = system.native_handle();
  granit_window_loop_desc recursive_desc = GRANIT_WINDOW_LOOP_DESC_INIT;
  recursive_desc.tick = raw_loop_tick;
  recursive_desc.shutdown = raw_loop_shutdown;
  recursive_desc.user_data = &recursive;
  recursive.desc = &recursive_desc;
  CHECK(granit_window_system_run_loop(recursive.system, &recursive_desc) == GRANIT_SUCCESS);
  CHECK(recursive.nested_result == GRANIT_ERROR_RESOURCE_IN_USE);
  CHECK(recursive.shutdowns == 1);
}

TEST_CASE("Window Loop 只能从 Window System 创建线程运行", "[window][loop][thread]") {
  granit::window_system system;
  const auto initialize_result = system.initialize();
  if (initialize_result == granit::result::backend_unavailable)
    SKIP("当前环境没有可用的 Window 后端");
  REQUIRE(initialize_result == granit::result::success);

  raw_loop_context context;
  context.system = system.native_handle();
  granit_window_loop_desc desc = GRANIT_WINDOW_LOOP_DESC_INIT;
  desc.tick = raw_loop_tick;
  desc.shutdown = raw_loop_shutdown;
  desc.user_data = &context;
  granit_result thread_result = GRANIT_ERROR_UNKNOWN;
  std::thread worker{[&] { thread_result = granit_window_system_run_loop(context.system, &desc); }};
  worker.join();
  CHECK(thread_result == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(context.shutdowns == 0);
}

TEST_CASE("C++ Input Event 把 C 事件转换为强类型字段", "[input][cpp]") {
  granit_input_event native = GRANIT_INPUT_EVENT_INIT;
  native.type = GRANIT_INPUT_EVENT_KEY;
  native.window = UINT64_C(42);
  native.timestamp_ns = UINT64_C(1234);
  native.data.key.physical_key = GRANIT_PHYSICAL_KEY_ESCAPE;
  native.data.key.logical_key = GRANIT_LOGICAL_KEY_ESCAPE;
  native.data.key.action = GRANIT_KEY_ACTION_RELEASED;

  const auto event = granit::detail::from_native(native);
  CHECK(event.type == granit::input_event_type::key);
  CHECK(event.window == native.window);
  CHECK(event.timestamp_ns == native.timestamp_ns);
  CHECK(event.data.key.physical == granit::physical_key::escape);
  CHECK(event.data.key.logical == granit::logical_key::escape);
  CHECK(event.data.key.action == granit::key_action::released);
}

TEST_CASE("Input 版本化输出不写越调用方容量", "[input][abi]") {
  check_versioned_output<granit_input_event, GRANIT_INPUT_EVENT_VERSION_1_SIZE>(
      [](granit_input_event* output) { return granit_window_poll_input_event(UINT64_MAX, output); },
      GRANIT_ERROR_INVALID_HANDLE);
  check_versioned_output<granit_keyboard_state, GRANIT_KEYBOARD_STATE_VERSION_1_SIZE>(
      [](granit_keyboard_state* output) {
        return granit_window_get_keyboard_state(UINT64_MAX, UINT64_C(1), output);
      },
      GRANIT_ERROR_INVALID_HANDLE);
  check_versioned_output<granit_pointer_state, GRANIT_POINTER_STATE_VERSION_1_SIZE>(
      [](granit_pointer_state* output) {
        return granit_window_get_pointer_state(UINT64_MAX, UINT64_C(1), output);
      },
      GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Window System 在当前平台直接拥有输入状态和事件队列", "[input][contract]") {
  granit_window_system_desc window_system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  granit_window_system window_system = GRANIT_NULL_HANDLE;
  const auto system_result = granit_window_system_create(&window_system_desc, &window_system);
  if (system_result == GRANIT_ERROR_BACKEND_UNAVAILABLE)
    SKIP("当前环境没有可用的 Window 后端");
  REQUIRE(system_result == GRANIT_SUCCESS);

  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.width = 96;
  window_desc.height = 72;
  window_desc.flags = 0;
  granit_window window = GRANIT_NULL_HANDLE;
  REQUIRE(granit_window_create(window_system, &window_desc, &window) == GRANIT_SUCCESS);

  granit_keyboard_state keyboard = GRANIT_KEYBOARD_STATE_INIT;
  granit_pointer_state pointer = GRANIT_POINTER_STATE_INIT;
  CHECK(granit_window_get_keyboard_state(window_system, window, &keyboard) == GRANIT_SUCCESS);
  CHECK(granit_window_get_pointer_state(window_system, window, &pointer) == GRANIT_SUCCESS);
  CHECK(keyboard.struct_size == sizeof(granit_keyboard_state));
  CHECK(pointer.struct_size == sizeof(granit_pointer_state));

  granit_input_event input_event = GRANIT_INPUT_EVENT_INIT;
  CHECK(granit_window_poll_input_event(window_system, &input_event) == GRANIT_ERROR_NOT_READY);

  REQUIRE(granit_window_destroy(window_system, window) == GRANIT_SUCCESS);
  keyboard.modifiers = UINT32_MAX;
  CHECK(granit_window_get_keyboard_state(window_system, window, &keyboard) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(keyboard.modifiers == 0);
  REQUIRE(granit_window_system_destroy(window_system) == GRANIT_SUCCESS);
}

TEST_CASE("Window创建把空Window System归类为无效句柄", "[window][contract]") {
  CHECK(granit_window_system_process_events(UINT64_MAX) == GRANIT_ERROR_INVALID_HANDLE);
  granit_surface surface = UINT64_C(42);
  CHECK(granit_window_create_surface(UINT64_MAX, UINT64_C(1), UINT64_C(1), &surface) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(surface == GRANIT_NULL_HANDLE);
  CHECK(granit_window_create_surface(UINT64_MAX, UINT64_C(1), UINT64_C(1), nullptr) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  granit_window_desc desc = GRANIT_WINDOW_DESC_INIT;
  desc.width = 1;
  desc.height = 1;
  granit_window handle = UINT64_C(1);
  CHECK(granit_window_create(GRANIT_NULL_HANDLE, &desc, &handle) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(handle == GRANIT_NULL_HANDLE);

}
