// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <backends/imgui_impl_sdl3.h>
#include <emscripten/emscripten.h>
#include <imgui.h>

#include "samples/imgui/content.h"
#include "samples/imgui/resources.h"
#include "imgui/imgui_theme.h"

#include <granit/granit.hpp>
#include <granit/integrations/imgui/renderer.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>

#include <cstdint>
#include <cstdio>

namespace {

enum class runtime_phase { renderer_pending, running, failed, stopped };

struct web_imgui_state {
  SDL_Window* window{};
  bool sdl_ready{};
  bool imgui_ready{};
  runtime_phase phase{runtime_phase::renderer_pending};
  granit::renderer renderer;
  granit::surface surface;
  granit::swapchain swapchain;
  granit::swapchain_info swapchain_info;
  granit::frame_context frame_context;
  granit::canvas_draw_list canvas;
  granit::texture font_texture;
  granit::texture_view font_view;
  granit::texture checker_texture;
  granit::texture_view checker_view;
  granit::sampler sampler;
  granit::example::imgui_sample_texture_bindings bindings;
  granit::example::imgui_sample_state sample;
  std::uint32_t tick_count{};
  std::uint32_t rendered_frames{};
  std::uint32_t pointer_events{};
  std::uint32_t resize_count{};
};

web_imgui_state state;

void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category, const char* message,
              std::uint32_t message_length, void*) noexcept {
  auto* stream = severity == GRANIT_DIAGNOSTIC_SEVERITY_INFO ? stdout : stderr;
  std::fprintf(stream, "GRANIT_DIAGNOSTIC:%.*s\n", static_cast<int>(message_length), message);
}

void report_failure(const char* stage, granit::result result) noexcept {
  state.phase = runtime_phase::failed;
  std::fprintf(stderr, "GRANIT_STATUS:failed:%s:%d\n", stage, static_cast<int>(result));
}

granit::result query_canvas_size(std::uint32_t& width, std::uint32_t& height) noexcept {
  int pixel_width{};
  int pixel_height{};
  if (!SDL_GetWindowSizeInPixels(state.window, &pixel_width, &pixel_height) || pixel_width <= 0 ||
      pixel_height <= 0) {
    return granit::result::not_ready;
  }
  width = static_cast<std::uint32_t>(pixel_width);
  height = static_cast<std::uint32_t>(pixel_height);
  return granit::result::success;
}

granit::result initialize_gpu_resources() {
  std::uint32_t width{};
  std::uint32_t height{};
  auto result = query_canvas_size(width, height);
  if (result.failed())
    return result;
  result = state.surface.initialize_canvas(state.renderer.native_handle());
  if (result.ok()) {
    result = state.swapchain.initialize(state.renderer.native_handle(), state.surface.native_handle(),
                                        {.width = width,
                                         .height = height,
                                         .minimum_image_count = 2,
                                         .presentation = granit::present_mode::fifo});
  }
  if (result.ok())
    result = state.swapchain.query_info(state.swapchain_info);
  if (result.ok())
    result = state.frame_context.initialize(state.renderer.native_handle());
  if (result.ok()) {
    granit_canvas_draw_list_desc desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
    desc.frame_slot_count = 2;
    result = state.canvas.initialize(state.renderer.native_handle(), desc);
  }
  if (result.ok()) {
    result = granit::example::upload_imgui_font_atlas(
        state.renderer.native_handle(), state.font_texture, state.font_view, state.sampler);
  }
  if (result.ok()) {
    result = granit::example::upload_imgui_checker_texture(
        state.renderer.native_handle(), state.checker_texture, state.checker_view);
  }
  if (result.ok()) {
    state.bindings = {
        .font = {state.font_view.native_handle(), state.sampler.native_handle()},
        .checker = {state.checker_view.native_handle(), state.sampler.native_handle()}};
  }
  return result;
}

granit::result resize_if_needed() {
  std::uint32_t width{};
  std::uint32_t height{};
  auto result = query_canvas_size(width, height);
  if (result.failed() || (width == state.swapchain_info.width &&
                          height == state.swapchain_info.height)) {
    return result;
  }
  result = state.swapchain.recreate(
      {.width = width, .height = height, .minimum_image_count = 2,
       .presentation = granit::present_mode::fifo});
  if (result.ok())
    result = state.swapchain.query_info(state.swapchain_info);
  if (result.ok())
    ++state.resize_count;
  return result;
}

granit::result render_frame() {
  const char* operation = "resize";
  auto result = resize_if_needed();
  if (result == granit::result::not_ready)
    return granit::result::success;
  if (result.failed())
    return result;

  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  granit::example::build_imgui_sample(
      state.sample,
      {.framebuffer_width = state.swapchain_info.width,
       .framebuffer_height = state.swapchain_info.height,
       .presentation = "FIFO",
       .show_custom_texture = true,
       .custom_texture = granit::example::imgui_checker_texture_id});
  ImGui::Render();

  operation = "canvas-clear";
  result = state.canvas.clear();
  if (result.ok()) {
    operation = "imgui-convert";
    result = granit::integration::imgui::append_draw_data(
        ImGui::GetDrawData(), state.canvas, granit::example::resolve_imgui_sample_texture,
        &state.bindings);
  }

  granit::acquired_frame frame;
  if (result.ok()) {
    operation = "acquire";
    result = state.swapchain.acquire(frame);
  }
  granit_texture backbuffer{};
  granit_texture_view view{};
  if (result.ok()) {
    operation = "backbuffer";
    result = state.swapchain.backbuffer(frame.image_index, backbuffer, view);
  }
  granit::frame_recording recording;
  if (result.ok()) {
    operation = "frame-begin";
    result = state.frame_context.begin(frame, recording);
  }
  if (result.ok()) {
    operation = "canvas-record";
    result = granit::example::record_imgui_sample_canvas(
        recording.recorder(), state.canvas, view, state.swapchain_info, recording.frame_slot());
  }
  if (result.ok()) {
    operation = "submit";
    result = recording.submit();
  }
  if (result.ok()) {
    operation = "present";
    result = state.swapchain.present(frame);
  }
  if (result.failed()) {
    std::fprintf(stderr, "GRANIT_DIAGNOSTIC:Web ImGui frame operation failed: %s\n", operation);
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(state.swapchain.cancel(frame));
  } else {
    ++state.rendered_frames;
  }
  return result;
}

void tick(void*) noexcept {
  ++state.tick_count;
  if (state.tick_count == 1) {
    std::puts("GRANIT_DIAGNOSTIC:Web ImGui frame loop started");
  } else if (state.tick_count == 2) {
    std::puts("GRANIT_DIAGNOSTIC:Web ImGui frame loop advancing");
  }
  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event.type == SDL_EVENT_MOUSE_BUTTON_UP || event.type == SDL_EVENT_MOUSE_WHEEL) {
      ++state.pointer_events;
    }
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      state.phase = runtime_phase::stopped;
      emscripten_cancel_main_loop();
      return;
    }
  }
  const auto process_result = state.renderer.process_events();
  if (process_result.failed()) {
    report_failure("renderer-events", process_result);
    return;
  }
  if (state.phase == runtime_phase::renderer_pending) {
    granit::renderer_status status;
    const auto result = state.renderer.get_status(status);
    if (result.failed()) {
      report_failure("renderer-status", result);
      return;
    }
    if (status.state == granit::renderer_state::initializing)
      return;
    if (status.state != granit::renderer_state::ready) {
      report_failure("renderer", status.failure_result);
      return;
    }
    const auto initialize_result = initialize_gpu_resources();
    if (initialize_result.failed()) {
      report_failure("resources", initialize_result);
      return;
    }
    state.phase = runtime_phase::running;
    std::puts("GRANIT_STATUS:ready");
  }
  if (state.phase != runtime_phase::running)
    return;
  const auto result = render_frame();
  if (result == granit::result::out_of_date)
    return;
  if (result.failed())
    report_failure("frame", result);
}

void shutdown() noexcept {
  state.phase = runtime_phase::stopped;
  static_cast<void>(state.checker_view.reset());
  static_cast<void>(state.checker_texture.reset());
  static_cast<void>(state.font_view.reset());
  static_cast<void>(state.font_texture.reset());
  static_cast<void>(state.sampler.reset());
  static_cast<void>(state.canvas.destroy());
  static_cast<void>(state.frame_context.reset());
  static_cast<void>(state.swapchain.reset());
  static_cast<void>(state.surface.reset());
  static_cast<void>(state.renderer.reset());
  if (state.imgui_ready) {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    state.imgui_ready = false;
  }
  if (state.window != nullptr) {
    SDL_DestroyWindow(state.window);
    state.window = nullptr;
  }
  if (state.sdl_ready) {
    SDL_Quit();
    state.sdl_ready = false;
  }
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_imgui_rendered_frames() noexcept {
  return state.rendered_frames;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_imgui_pointer_events() noexcept {
  return state.pointer_events;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_imgui_resize_count() noexcept {
  return state.resize_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_imgui_shutdown() noexcept {
  emscripten_cancel_main_loop();
  shutdown();
  return 0;
}

int main() {
  std::puts("GRANIT_DIAGNOSTIC:Web ImGui main started");
  SDL_SetMainReady();
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "GRANIT_STATUS:failed:sdl:%s\n", SDL_GetError());
    return 1;
  }
  std::puts("GRANIT_DIAGNOSTIC:SDL3 video ready");
  state.sdl_ready = true;
  state.window = SDL_CreateWindow("Granit SDL3 + ImGui | WebGPU", 1280, 720,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (state.window == nullptr) {
    std::fprintf(stderr, "GRANIT_STATUS:failed:window:%s\n", SDL_GetError());
    shutdown();
    return 1;
  }
  std::puts("GRANIT_DIAGNOSTIC:SDL3 window ready");
  ImGui::CreateContext();
  granit::example::apply_imgui_theme();
  if (!ImGui_ImplSDL3_InitForOther(state.window)) {
    std::fputs("GRANIT_STATUS:failed:imgui-platform\n", stderr);
    ImGui::DestroyContext();
    shutdown();
    return 1;
  }
  state.imgui_ready = true;
  std::puts("GRANIT_DIAGNOSTIC:ImGui platform ready");
  const auto result = state.renderer.initialize(
      {.application_name = "Granit Web ImGui",
       .surface_types = granit::surface_type::canvas,
       .frames_in_flight = 2,
       .diagnostics = diagnose,
       .backend = granit::renderer_backend::webgpu});
  if (result.failed()) {
    report_failure("renderer-create", result);
    shutdown();
    return 1;
  }
  std::puts("GRANIT_DIAGNOSTIC:WebGPU renderer requested");
  emscripten_set_main_loop_arg(tick, nullptr, 0, true);
  return 0;
}
