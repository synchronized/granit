// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>

#include "backend/callback_lifetime.h"
#include "backend/lifecycle.h"

namespace {

enum class startup_status : int { failed = -1, starting, adapter_pending, device_pending, ready };

struct web_platform_state {
  WGPUInstance instance{};
  WGPUSurface surface{};
  WGPUAdapter adapter{};
  WGPUDevice device{};
  startup_status status{startup_status::starting};
  unsigned input_event_count{};
  granit::detail::backend_lifecycle lifecycle;
  granit::detail::backend_callback_lifetime callback_lifetime;
  granit::detail::backend_callback_ticket adapter_ticket;
  granit::detail::backend_callback_ticket device_ticket;
};

web_platform_state state;

void fail(const char* message) noexcept {
  state.status = startup_status::failed;
  state.lifecycle.mark_failed(GRANIT_ERROR_INITIALIZATION_FAILED);
  std::fprintf(stderr, "GRANIT_STATUS:failed:%s\n", message);
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

EM_BOOL receive_keyboard(int, const EmscriptenKeyboardEvent*, void* user_data) noexcept {
  ++static_cast<web_platform_state*>(user_data)->input_event_count;
  return EM_FALSE;
}

EM_BOOL receive_mouse(int, const EmscriptenMouseEvent*, void* user_data) noexcept {
  ++static_cast<web_platform_state*>(user_data)->input_event_count;
  return EM_FALSE;
}

bool configure_surface() noexcept {
  WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
  if (wgpuSurfaceGetCapabilities(state.surface, state.adapter, &capabilities) !=
          WGPUStatus_Success ||
      capabilities.formatCount == 0 || capabilities.presentModeCount == 0 ||
      capabilities.alphaModeCount == 0) {
    wgpuSurfaceCapabilitiesFreeMembers(capabilities);
    return false;
  }

  int width{};
  int height{};
  if (emscripten_get_canvas_element_size("#canvas", &width, &height) != EMSCRIPTEN_RESULT_SUCCESS ||
      width <= 0 || height <= 0) {
    wgpuSurfaceCapabilitiesFreeMembers(capabilities);
    return false;
  }

  WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
  configuration.device = state.device;
  configuration.format = capabilities.formats[0];
  configuration.usage = WGPUTextureUsage_RenderAttachment;
  configuration.width = static_cast<std::uint32_t>(width);
  configuration.height = static_cast<std::uint32_t>(height);
  configuration.presentMode = capabilities.presentModes[0];
  configuration.alphaMode = capabilities.alphaModes[0];
  wgpuSurfaceConfigure(state.surface, &configuration);
  wgpuSurfaceCapabilitiesFreeMembers(capabilities);
  return true;
}

void receive_device(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* data,
                    void*) noexcept {
  const auto& ticket = *static_cast<granit::detail::backend_callback_ticket*>(data);
  static_cast<void>(ticket.invoke([status, device] {
    if (status != WGPURequestDeviceStatus_Success || device == nullptr) {
      fail("device-request");
      return;
    }
    state.device = device;
    if (!configure_surface()) {
      fail("surface-configure");
      return;
    }
    state.status = startup_status::ready;
    state.lifecycle.mark_ready();
    std::puts("GRANIT_STATUS:ready");
  }));
}

void receive_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView,
                     void* data, void*) noexcept {
  const auto& ticket = *static_cast<granit::detail::backend_callback_ticket*>(data);
  static_cast<void>(ticket.invoke([status, adapter] {
    if (status != WGPURequestAdapterStatus_Success || adapter == nullptr) {
      fail("adapter-request");
      return;
    }
    state.adapter = adapter;
    state.status = startup_status::device_pending;
    WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.callback = receive_device;
    callback.userdata1 = &state.device_ticket;
    static_cast<void>(wgpuAdapterRequestDevice(state.adapter, nullptr, callback));
  }));
}

void tick(void*) noexcept {
  if (state.instance != nullptr) {
    wgpuInstanceProcessEvents(state.instance);
  }
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

  WGPUInstanceDescriptor instance_descriptor = WGPU_INSTANCE_DESCRIPTOR_INIT;
  state.instance = wgpuCreateInstance(&instance_descriptor);
  if (state.instance == nullptr) {
    fail("instance-create");
    return 1;
  }

  WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_source =
      WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
  canvas_source.selector = {"#canvas", WGPU_STRLEN};
  WGPUSurfaceDescriptor surface_descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
  surface_descriptor.nextInChain = &canvas_source.chain;
  state.surface = wgpuInstanceCreateSurface(state.instance, &surface_descriptor);
  if (state.surface == nullptr) {
    fail("surface-create");
    return 1;
  }

  static_cast<void>(emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &state,
                                                    EM_FALSE, receive_keyboard));
  static_cast<void>(emscripten_set_mousedown_callback("#canvas", &state, EM_FALSE, receive_mouse));
  static_cast<void>(emscripten_set_mousemove_callback("#canvas", &state, EM_FALSE, receive_mouse));

  WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
  options.compatibleSurface = state.surface;
  WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
  callback.mode = WGPUCallbackMode_AllowSpontaneous;
  callback.callback = receive_adapter;
  state.adapter_ticket = state.callback_lifetime.ticket();
  state.device_ticket = state.callback_lifetime.ticket();
  callback.userdata1 = &state.adapter_ticket;
  state.status = startup_status::adapter_pending;
  static_cast<void>(wgpuInstanceRequestAdapter(state.instance, &options, callback));

  emscripten_set_main_loop_arg(tick, &state, 0, EM_FALSE);
  return 0;
}
