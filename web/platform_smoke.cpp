// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <granit/renderer/command_recorder.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/shader.h>
#include <granit/renderer/surface.h>
#include <granit/renderer/swapchain.h>

namespace {

enum class startup_status : int { failed = -1, starting, provider_pending, ready };

struct web_platform_state {
  granit_renderer renderer{};
  granit_surface surface{};
  granit_swapchain swapchain{};
  startup_status status{startup_status::starting};
  unsigned input_event_count{};
};

web_platform_state state;

void fail(const char* message, granit_result result = GRANIT_ERROR_INITIALIZATION_FAILED) noexcept {
  state.status = startup_status::failed;
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

granit_result validate_public_pipeline() {
  constexpr char vertex_wgsl[] = R"(
@vertex fn main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  var positions = array<vec2f, 3>(vec2f(0.0, 0.5), vec2f(-0.5, -0.5), vec2f(0.5, -0.5));
  return vec4f(positions[index], 0.0, 1.0);
})";
  constexpr char fragment_wgsl[] = R"(
@fragment fn main() -> @location(0) vec4f {
  return vec4f(0.0, 1.0, 0.0, 1.0);
})";
  granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  desc.code = nullptr;
  desc.code_size = 0;
  desc.wgsl = vertex_wgsl;
  desc.wgsl_length = sizeof(vertex_wgsl) - 1;
  granit_shader vertex{};
  auto result = granit_shader_create(state.renderer, &desc, &vertex);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  desc.stage = GRANIT_SHADER_STAGE_FRAGMENT;
  desc.wgsl = fragment_wgsl;
  desc.wgsl_length = sizeof(fragment_wgsl) - 1;
  granit_shader fragment{};
  result = granit_shader_create(state.renderer, &desc, &fragment);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_shader_destroy(state.renderer, vertex));
    return result;
  }
  granit_pipeline_layout_desc layout_desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
  granit_pipeline_layout layout{};
  result = granit_pipeline_layout_create(state.renderer, &layout_desc, &layout);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_shader_destroy(state.renderer, fragment));
    static_cast<void>(granit_shader_destroy(state.renderer, vertex));
    return result;
  }
  constexpr granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  granit_graphics_pipeline_desc pipeline_desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  pipeline_desc.layout = layout;
  pipeline_desc.vertex_shader = vertex;
  pipeline_desc.fragment_shader = fragment;
  pipeline_desc.color_format_count = 1;
  pipeline_desc.color_formats = &color_format;
  granit_graphics_pipeline pipeline{};
  result = granit_graphics_pipeline_create(state.renderer, &pipeline_desc, &pipeline);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_pipeline_layout_destroy(state.renderer, layout));
    static_cast<void>(granit_shader_destroy(state.renderer, fragment));
    static_cast<void>(granit_shader_destroy(state.renderer, vertex));
    return result;
  }
  if (granit_shader_destroy(state.renderer, vertex) != GRANIT_SUCCESS ||
      granit_pipeline_layout_destroy(state.renderer, layout) != GRANIT_SUCCESS) {
    return GRANIT_ERROR_INTERNAL;
  }
  result = granit_graphics_pipeline_destroy(state.renderer, pipeline);
  if (result == GRANIT_SUCCESS) {
    result = granit_shader_destroy(state.renderer, fragment);
  }
  if (result != GRANIT_SUCCESS ||
      granit_shader_destroy(state.renderer, vertex) != GRANIT_ERROR_INVALID_HANDLE) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INTERNAL : result;
  }
  return GRANIT_SUCCESS;
}

granit_result draw_public_triangle(granit_frame frame, granit_texture_view view,
                                   granit_texture_format format, std::uint32_t width,
                                   std::uint32_t height) {
  constexpr char vertex_wgsl[] = R"(
@vertex fn main(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  var positions = array<vec2f, 3>(vec2f(0.0, 0.6), vec2f(-0.6, -0.6), vec2f(0.6, -0.6));
  return vec4f(positions[index], 0.0, 1.0);
})";
  constexpr char fragment_wgsl[] = R"(
@fragment fn main() -> @location(0) vec4f {
  return vec4f(0.0, 1.0, 0.0, 1.0);
})";
  granit_shader_desc shader_desc = GRANIT_SHADER_DESC_INIT;
  shader_desc.code = nullptr;
  shader_desc.code_size = 0;
  shader_desc.wgsl = vertex_wgsl;
  shader_desc.wgsl_length = sizeof(vertex_wgsl) - 1;
  granit_shader vertex{};
  auto result = granit_shader_create(state.renderer, &shader_desc, &vertex);
  if (result != GRANIT_SUCCESS)
    return result;
  shader_desc.stage = GRANIT_SHADER_STAGE_FRAGMENT;
  shader_desc.wgsl = fragment_wgsl;
  shader_desc.wgsl_length = sizeof(fragment_wgsl) - 1;
  granit_shader fragment{};
  result = granit_shader_create(state.renderer, &shader_desc, &fragment);
  granit_pipeline_layout layout{};
  granit_graphics_pipeline pipeline{};
  granit_command_recorder recorder{};
  if (result == GRANIT_SUCCESS) {
    const granit_pipeline_layout_desc desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
    result = granit_pipeline_layout_create(state.renderer, &desc, &layout);
  }
  if (result == GRANIT_SUCCESS) {
    granit_graphics_pipeline_desc desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
    desc.layout = layout;
    desc.vertex_shader = vertex;
    desc.fragment_shader = fragment;
    desc.color_format_count = 1;
    desc.color_formats = &format;
    result = granit_graphics_pipeline_create(state.renderer, &desc, &pipeline);
  }
  if (result == GRANIT_SUCCESS) {
    const granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
    result = granit_command_recorder_create(state.renderer, &desc, &recorder);
  }
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_begin(state.renderer, recorder);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = view;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.area = {0, 0, width, height};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_begin_rendering(state.renderer, recorder, &rendering);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_bind_graphics_pipeline(state.renderer, recorder, pipeline);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_draw(state.renderer, recorder, 3, 1, 0, 0);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end_rendering(state.renderer, recorder);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end(state.renderer, recorder);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_submit_frame(state.renderer, recorder, frame);
  if (recorder != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_command_recorder_destroy(state.renderer, recorder));
  if (pipeline != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_graphics_pipeline_destroy(state.renderer, pipeline));
  if (layout != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_pipeline_layout_destroy(state.renderer, layout));
  if (fragment != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_shader_destroy(state.renderer, fragment));
  static_cast<void>(granit_shader_destroy(state.renderer, vertex));
  return result;
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

  granit_canvas_surface_desc surface_desc = GRANIT_CANVAS_SURFACE_DESC_INIT;
  auto result = granit_surface_create_canvas(state.renderer, &surface_desc, &state.surface);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  granit_swapchain_desc swapchain_desc = GRANIT_SWAPCHAIN_DESC_INIT;
  swapchain_desc.width = static_cast<std::uint32_t>(width);
  swapchain_desc.height = static_cast<std::uint32_t>(height);
  swapchain_desc.minimum_image_count = 2;
  result =
      granit_swapchain_create(state.renderer, state.surface, &swapchain_desc, &state.swapchain);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;
  result = granit_swapchain_get_info(state.renderer, state.swapchain, &info);
  if (result != GRANIT_SUCCESS || info.width == 0 || info.height == 0 || info.image_count == 0) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  granit_frame frame{};
  std::uint32_t image_index{};
  std::uint32_t needs_recreate{};
  result = granit_swapchain_acquire(state.renderer, state.swapchain, &frame, &image_index,
                                    &needs_recreate);
  if (result != GRANIT_SUCCESS || frame == GRANIT_NULL_HANDLE) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  granit_texture texture{};
  granit_texture_view view{};
  result = granit_swapchain_get_backbuffer(state.renderer, state.swapchain, image_index, &texture,
                                           &view);
  if (result != GRANIT_SUCCESS || texture == GRANIT_NULL_HANDLE || view == GRANIT_NULL_HANDLE) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  granit_frame_info frame_info = GRANIT_FRAME_INFO_INIT;
  result = granit_frame_get_info(state.renderer, state.swapchain, frame, &frame_info);
  if (result != GRANIT_SUCCESS || frame_info.frame_slot_count == 0) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INITIALIZATION_FAILED : result;
  }
  result = granit_frame_cancel(state.renderer, state.swapchain, frame, &needs_recreate);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  if (granit_frame_get_info(state.renderer, state.swapchain, frame, &frame_info) !=
          GRANIT_ERROR_INVALID_HANDLE ||
      granit_swapchain_get_backbuffer(state.renderer, state.swapchain, image_index, &texture,
                                      &view) != GRANIT_ERROR_INVALID_ARGUMENT) {
    return GRANIT_ERROR_INTERNAL;
  }
  result = granit_swapchain_acquire(state.renderer, state.swapchain, &frame, &image_index,
                                    &needs_recreate);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  result = granit_swapchain_get_backbuffer(state.renderer, state.swapchain, image_index, &texture,
                                           &view);
  if (result == GRANIT_SUCCESS) {
    result = draw_public_triangle(frame, view, info.format, info.width, info.height);
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_frame_cancel(state.renderer, state.swapchain, frame, &needs_recreate));
    return result;
  }
  result = granit_swapchain_present(state.renderer, state.swapchain, frame, &needs_recreate);
  if (result != GRANIT_SUCCESS ||
      granit_frame_get_info(state.renderer, state.swapchain, frame, &frame_info) !=
          GRANIT_ERROR_INVALID_HANDLE) {
    return result == GRANIT_SUCCESS ? GRANIT_ERROR_INTERNAL : result;
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
  const auto pipeline_result = validate_public_pipeline();
  if (pipeline_result != GRANIT_SUCCESS) {
    fail("renderer-pipeline", pipeline_result);
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
