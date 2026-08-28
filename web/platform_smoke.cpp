// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <granit/renderer/renderer.h>

#include "backend/lifecycle.h"
#include "renderer_registry.h"

namespace {

enum class startup_status : int { failed = -1, starting, provider_pending, ready };

struct web_platform_state {
  granit_renderer renderer{};
  std::shared_ptr<granit::detail::webgpu_renderer_state> backend;
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

  auto* presentation = state.backend->presentation();
  if (presentation == nullptr) {
    return GRANIT_ERROR_NOT_READY;
  }
  state.surface = presentation->allocate_surface();
  state.swapchain = presentation->allocate_swapchain();
  if (state.surface == nullptr || state.swapchain == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  auto result = presentation->create_canvas_surface(*state.surface, "#canvas", 7);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  const granit::detail::backend_swapchain_desc desc{static_cast<std::uint32_t>(width),
                                                    static_cast<std::uint32_t>(height), 2,
                                                    GRANIT_BACKEND_PLUGIN_PRESENT_MODE_FIFO};
  result = presentation->create_swapchain(*state.surface, desc, *state.swapchain);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  granit::detail::backend_swapchain_info info{};
  result = presentation->get_swapchain_info(*state.swapchain, info);
  if (result != GRANIT_SUCCESS || info.width == 0 || info.height == 0 || info.image_count == 0) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  granit::detail::backend_acquired_swapchain_frame frame{};
  result = presentation->acquire_swapchain(*state.swapchain, frame);
  if (result != GRANIT_SUCCESS || frame.dynamic_backbuffer.texture == nullptr ||
      frame.dynamic_backbuffer.view == nullptr) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  bool needs_recreate{};
  result = presentation->cancel_swapchain(*state.swapchain, needs_recreate);
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
  const auto process_result = granit_renderer_process_events(state.renderer);
  if (process_result != GRANIT_SUCCESS) {
    fail("provider-events", process_result);
    return;
  }

  granit_renderer_status renderer_status = GRANIT_RENDERER_STATUS_INIT;
  const auto status_result = granit_renderer_get_status(state.renderer, &renderer_status);
  if (status_result != GRANIT_SUCCESS) {
    fail("renderer-status", status_result);
    return;
  }
  if (renderer_status.state == GRANIT_RENDERER_STATE_FAILED ||
      renderer_status.state == GRANIT_RENDERER_STATE_DEVICE_LOST) {
    fail("provider-terminal", renderer_status.failure_result);
    return;
  }
  if (renderer_status.state != GRANIT_RENDERER_STATE_READY) {
    return;
  }

  granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  const auto limits_result = granit_renderer_get_limits(state.renderer, &limits);
  if (limits_result != GRANIT_SUCCESS || limits.uniform_buffer_offset_alignment == 0 ||
      limits.max_uniform_buffer_binding_size == 0) {
    fail("renderer-limits",
         limits_result == GRANIT_SUCCESS ? GRANIT_ERROR_INTERNAL : limits_result);
    return;
  }
  state.backend = granit::detail::web_renderer_registry::instance().acquire(state.renderer);
  if (!state.backend) {
    fail("renderer-acquire", GRANIT_ERROR_INVALID_HANDLE);
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
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  return granit_renderer_get_status(state.renderer, &status) == GRANIT_SUCCESS ? status.state : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_renderer_failure_result() noexcept {
  granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
  return granit_renderer_get_status(state.renderer, &status) == GRANIT_SUCCESS
             ? status.failure_result
             : GRANIT_ERROR_INVALID_HANDLE;
}

int main() {
  if (!load_startup_resource()) {
    fail("preloaded-resource");
    return 1;
  }

  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.surface_types = GRANIT_SURFACE_TYPE_CANVAS_BIT;
  desc.diagnostic_callback = diagnose;
  const auto result = granit_renderer_create(&desc, &state.renderer);
  if (result != GRANIT_SUCCESS) {
    fail("provider-open", result);
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
