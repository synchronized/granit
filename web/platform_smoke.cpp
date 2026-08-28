// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "backend/lifecycle.h"
#include "backend/plugin_loader.h"
#include "backend/webgpu/presentation_adapter.h"

extern "C" const granit_backend_plugin_api*
granit_backend_plugin_query(std::uint32_t requested_abi) noexcept;

namespace {

enum class startup_status : int { failed = -1, starting, provider_pending, ready };

struct web_platform_state {
  granit::detail::backend_plugin_loader loader;
  granit_backend_plugin_instance instance{};
  std::unique_ptr<granit::detail::webgpu_presentation_adapter> presentation;
  std::unique_ptr<granit::detail::backend_surface_resource> surface;
  std::unique_ptr<granit::detail::backend_swapchain_resource> swapchain;
  startup_status status{startup_status::starting};
  unsigned input_event_count{};
  granit::detail::backend_lifecycle lifecycle;
};

web_platform_state state;

void fail(const char* message, granit_result result = GRANIT_ERROR_INITIALIZATION_FAILED) noexcept {
  state.status = startup_status::failed;
  state.lifecycle.mark_failed(result);
  std::fprintf(stderr, "GRANIT_STATUS:failed:%s:%d\n", message, result);
}

bool load_startup_resource() noexcept {
  auto* file = std::fopen("/assets/s10d_startup.txt", "rb");
  if (file == nullptr) {
    return false;
  }
  char content[64]{};
  const auto size = std::fread(content, 1, sizeof(content) - 1, file);
  std::fclose(file);
  constexpr char expected[] = "granit-s10d-web-platform";
  return size >= sizeof(expected) - 1 && std::memcmp(content, expected, sizeof(expected) - 1) == 0;
}

void* allocate(std::uint64_t size, std::uint64_t alignment, void*) noexcept {
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void deallocate(void* memory, std::uint64_t, std::uint64_t alignment, void*) noexcept {
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void diagnose(granit_diagnostic_severity, granit_diagnostic_category, const char* message,
              std::uint32_t message_length, void*) noexcept {
  std::fprintf(stderr, "GRANIT_DIAGNOSTIC:%.*s\n", static_cast<int>(message_length), message);
}

EM_BOOL receive_keyboard(int, const EmscriptenKeyboardEvent*, void* user_data) noexcept {
  ++static_cast<web_platform_state*>(user_data)->input_event_count;
  return EM_FALSE;
}

EM_BOOL receive_mouse(int, const EmscriptenMouseEvent*, void* user_data) noexcept {
  ++static_cast<web_platform_state*>(user_data)->input_event_count;
  return EM_FALSE;
}

granit_result create_presentation_resources() {
  int width{};
  int height{};
  if (emscripten_get_canvas_element_size("#canvas", &width, &height) != EMSCRIPTEN_RESULT_SUCCESS ||
      width <= 0 || height <= 0) {
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  state.presentation =
      std::make_unique<granit::detail::webgpu_presentation_adapter>(state.loader, state.instance);
  state.surface = state.presentation->allocate_surface();
  state.swapchain = state.presentation->allocate_swapchain();
  if (state.surface == nullptr || state.swapchain == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  auto result = state.presentation->create_canvas_surface(*state.surface, "#canvas", 7);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  const granit::detail::backend_swapchain_desc desc{static_cast<std::uint32_t>(width),
                                                    static_cast<std::uint32_t>(height), 2,
                                                    GRANIT_BACKEND_PLUGIN_PRESENT_MODE_FIFO};
  result = state.presentation->create_swapchain(*state.surface, desc, *state.swapchain);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  granit::detail::backend_swapchain_info info{};
  result = state.presentation->get_swapchain_info(*state.swapchain, info);
  if (result != GRANIT_SUCCESS || info.width == 0 || info.height == 0 || info.image_count == 0) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  granit::detail::backend_acquired_swapchain_frame frame{};
  result = state.presentation->acquire_swapchain(*state.swapchain, frame);
  if (result != GRANIT_SUCCESS || frame.dynamic_backbuffer.texture == nullptr ||
      frame.dynamic_backbuffer.view == nullptr) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  bool needs_recreate{};
  result = state.presentation->cancel_swapchain(*state.swapchain, needs_recreate);
  frame = {};
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  return GRANIT_SUCCESS;
}

void tick(void*) noexcept {
  if (state.status != startup_status::provider_pending) {
    return;
  }
  const auto process_result = state.loader.process_events(state.instance);
  if (process_result != GRANIT_SUCCESS && process_result != GRANIT_ERROR_NOT_READY) {
    fail("provider-events", process_result);
    return;
  }

  granit_backend_plugin_instance_status provider_status{};
  provider_status.struct_size = sizeof(provider_status);
  const auto status_result = state.loader.get_instance_status(state.instance, &provider_status);
  if (status_result != GRANIT_SUCCESS) {
    fail("provider-status", status_result);
    return;
  }
  if (provider_status.state == GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_FAILED ||
      provider_status.state == GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_DEVICE_LOST) {
    fail("provider-terminal", provider_status.failure_result);
    return;
  }
  if (provider_status.state != GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_READY) {
    return;
  }

  try {
    const auto result = create_presentation_resources();
    if (result != GRANIT_SUCCESS) {
      fail("presentation-create", result);
      return;
    }
  } catch (const std::bad_alloc&) {
    fail("presentation-allocation", GRANIT_ERROR_OUT_OF_MEMORY);
    return;
  } catch (...) {
    fail("presentation-exception", GRANIT_ERROR_INTERNAL);
    return;
  }
  state.status = startup_status::ready;
  state.lifecycle.mark_ready();
  std::puts("GRANIT_STATUS:ready");
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_platform_status() noexcept {
  return static_cast<int>(state.status);
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_input_event_count() noexcept {
  return state.input_event_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_renderer_state() noexcept {
  return static_cast<unsigned>(state.lifecycle.status().state);
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_renderer_failure_result() noexcept {
  return state.lifecycle.status().failure_result;
}

int main() {
  if (!load_startup_resource()) {
    fail("preloaded-resource");
    return 1;
  }

  const auto* api = granit_backend_plugin_query(GRANIT_BACKEND_PLUGIN_ABI_VERSION);
  auto result = state.loader.open_static(api, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU);
  if (result != GRANIT_SUCCESS) {
    fail("provider-open", result);
    return 1;
  }
  granit_backend_plugin_host_api host{sizeof(host), 0,          diagnose, nullptr,
                                      allocate,     deallocate, nullptr};
  result = state.loader.create_instance(&host, &state.instance);
  if (result != GRANIT_SUCCESS) {
    fail("provider-create", result);
    return 1;
  }

  static_cast<void>(emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &state,
                                                    EM_FALSE, receive_keyboard));
  static_cast<void>(emscripten_set_mousedown_callback("#canvas", &state, EM_FALSE, receive_mouse));
  static_cast<void>(emscripten_set_mousemove_callback("#canvas", &state, EM_FALSE, receive_mouse));

  state.status = startup_status::provider_pending;
  emscripten_set_main_loop_arg(tick, &state, 0, EM_FALSE);
  return 0;
}
