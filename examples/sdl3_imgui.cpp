// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <SDL3/SDL.h>

#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <granit/granit.hpp>
#include <granit/integrations/imgui/renderer.hpp>
#include <granit/integrations/sdl3/surface.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <vector>

namespace {

constexpr ImTextureID font_texture_id = 1;
constexpr ImTextureID checker_texture_id = 2;
constexpr std::uint32_t default_frame_slot_count = 3;

struct frame_timings {
  double cpu_ms{};
  double gpu_ms{};
  double present_ms{};
  double slot_wait_ms{};
  double imgui_ms{};
  double convert_ms{};
  double render_ms{};
  double acquire_ms{};
  double canvas_record_ms{};
  double submit_ms{};
};

void smooth(double& value, double sample) {
  constexpr double weight = 0.1;
  value = value == 0 ? sample : value + (sample - value) * weight;
}

struct sdl_quit {
  ~sdl_quit() { SDL_Quit(); }
};

struct window_deleter {
  void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};

struct imgui_quit {
  ~imgui_quit() {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
};

struct texture_binding {
  granit_texture_view view{GRANIT_NULL_HANDLE};
  granit_sampler sampler{GRANIT_NULL_HANDLE};
};

struct texture_bindings {
  texture_binding font;
  texture_binding checker;
};

void apply_granit_theme() {
  ImGui::StyleColorsDark();
  auto& style = ImGui::GetStyle();
  style.WindowPadding = {12, 10};
  style.FramePadding = {9, 5};
  style.ItemSpacing = {8, 7};
  style.ItemInnerSpacing = {7, 5};
  style.WindowRounding = 8;
  style.ChildRounding = 6;
  style.FrameRounding = 5;
  style.PopupRounding = 6;
  style.ScrollbarRounding = 9;
  style.GrabRounding = 5;
  style.TabRounding = 5;
  style.WindowBorderSize = 1;
  style.FrameBorderSize = 0;
  style.ScrollbarSize = 13;
  style.GrabMinSize = 10;

  auto* colors = style.Colors;
  colors[ImGuiCol_Text] = {0.88F, 0.91F, 0.96F, 1};
  colors[ImGuiCol_TextDisabled] = {0.47F, 0.52F, 0.63F, 1};
  colors[ImGuiCol_WindowBg] = {0.055F, 0.067F, 0.10F, 0.98F};
  colors[ImGuiCol_ChildBg] = {0.071F, 0.086F, 0.125F, 1};
  colors[ImGuiCol_PopupBg] = {0.071F, 0.086F, 0.125F, 0.98F};
  colors[ImGuiCol_Border] = {0.18F, 0.21F, 0.30F, 1};
  colors[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
  colors[ImGuiCol_FrameBg] = {0.105F, 0.125F, 0.18F, 1};
  colors[ImGuiCol_FrameBgHovered] = {0.15F, 0.18F, 0.27F, 1};
  colors[ImGuiCol_FrameBgActive] = {0.19F, 0.22F, 0.33F, 1};
  colors[ImGuiCol_TitleBg] = {0.071F, 0.086F, 0.125F, 1};
  colors[ImGuiCol_TitleBgActive] = {0.105F, 0.125F, 0.18F, 1};
  colors[ImGuiCol_TitleBgCollapsed] = {0.055F, 0.067F, 0.10F, 0.92F};
  colors[ImGuiCol_MenuBarBg] = {0.071F, 0.086F, 0.125F, 1};
  colors[ImGuiCol_ScrollbarBg] = {0.055F, 0.067F, 0.10F, 0.75F};
  colors[ImGuiCol_ScrollbarGrab] = {0.20F, 0.23F, 0.34F, 1};
  colors[ImGuiCol_ScrollbarGrabHovered] = {0.29F, 0.33F, 0.48F, 1};
  colors[ImGuiCol_ScrollbarGrabActive] = {0.43F, 0.36F, 0.86F, 1};
  colors[ImGuiCol_CheckMark] = {0.52F, 0.45F, 0.96F, 1};
  colors[ImGuiCol_SliderGrab] = {0.43F, 0.36F, 0.86F, 1};
  colors[ImGuiCol_SliderGrabActive] = {0.62F, 0.55F, 1, 1};
  colors[ImGuiCol_Button] = {0.36F, 0.29F, 0.76F, 1};
  colors[ImGuiCol_ButtonHovered] = {0.46F, 0.38F, 0.92F, 1};
  colors[ImGuiCol_ButtonActive] = {0.55F, 0.47F, 1, 1};
  colors[ImGuiCol_Header] = {0.27F, 0.23F, 0.52F, 0.72F};
  colors[ImGuiCol_HeaderHovered] = {0.38F, 0.31F, 0.75F, 0.82F};
  colors[ImGuiCol_HeaderActive] = {0.46F, 0.38F, 0.92F, 1};
  colors[ImGuiCol_Separator] = {0.18F, 0.21F, 0.30F, 1};
  colors[ImGuiCol_SeparatorHovered] = {0.43F, 0.36F, 0.86F, 1};
  colors[ImGuiCol_SeparatorActive] = {0.62F, 0.55F, 1, 1};
  colors[ImGuiCol_ResizeGrip] = {0.43F, 0.36F, 0.86F, 0.25F};
  colors[ImGuiCol_ResizeGripHovered] = {0.52F, 0.45F, 0.96F, 0.70F};
  colors[ImGuiCol_ResizeGripActive] = {0.62F, 0.55F, 1, 1};
  colors[ImGuiCol_TextSelectedBg] = {0.43F, 0.36F, 0.86F, 0.38F};
  colors[ImGuiCol_NavHighlight] = {0.62F, 0.55F, 1, 1};
  colors[ImGuiCol_ModalWindowDimBg] = {0.02F, 0.025F, 0.04F, 0.72F};

  // ImGui 色板按显示空间书写；Canvas 顶点色使用线性空间。
  const auto to_linear = [](float value) {
    return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
  };
  for (int index = 0; index < ImGuiCol_COUNT; ++index) {
    auto& color = colors[index];
    color.x = to_linear(color.x);
    color.y = to_linear(color.y);
    color.z = to_linear(color.z);
  }
}

granit::result resolve_texture(ImTextureID texture, granit_canvas_draw_state& state,
                               void* user_data) noexcept {
  if (user_data == nullptr)
    return granit::result::invalid_argument;
  const auto& bindings = *static_cast<const texture_bindings*>(user_data);
  const auto* binding = texture == font_texture_id      ? &bindings.font
                        : texture == checker_texture_id ? &bindings.checker
                                                        : nullptr;
  if (binding == nullptr)
    return granit::result::invalid_argument;
  state.texture = binding->view;
  state.sampler = binding->sampler;
  return granit::result::success;
}

granit::result upload_checker_texture(granit_renderer renderer, granit::texture& texture,
                                      granit::texture_view& view) {
  constexpr std::array<std::uint8_t, 16> pixels{238, 194, 255, 255, 35,  31, 52,  255,
                                                35,  31,  52,  255, 104, 87, 204, 255};
  auto result = texture.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                              .usage = granit::texture_usage::sampled |
                                                       granit::texture_usage::transfer_destination,
                                              .width = 2,
                                              .height = 2});
  if (granit::succeeded(result)) {
    result = texture.write(std::as_bytes(std::span{pixels}),
                           {.bytes_per_row = 8, .rows_per_image = 2}, {.width = 2, .height = 2});
  }
  if (granit::succeeded(result))
    result = view.initialize(renderer, texture.native_handle());
  return result;
}

granit::result upload_font_atlas(granit_renderer renderer, granit::texture& texture,
                                 granit::texture_view& view, granit::sampler& sampler) {
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  if (pixels == nullptr || width <= 0 || height <= 0)
    return granit::result::internal;

  const auto byte_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  std::vector<std::byte> premultiplied_pixels(byte_count);
  for (std::size_t offset = 0; offset < byte_count; offset += 4) {
    const auto alpha = pixels[offset + 3];
    for (std::size_t channel = 0; channel < 3; ++channel) {
      premultiplied_pixels[offset + channel] = static_cast<std::byte>(
          (static_cast<std::uint32_t>(pixels[offset + channel]) * alpha + 127U) / 255U);
    }
    premultiplied_pixels[offset + 3] = static_cast<std::byte>(alpha);
  }

  auto result = texture.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                              .usage = granit::texture_usage::sampled |
                                                       granit::texture_usage::transfer_destination,
                                              .width = static_cast<std::uint32_t>(width),
                                              .height = static_cast<std::uint32_t>(height)});
  if (granit::succeeded(result)) {
    const granit::texture_data_layout layout{.bytes_per_row = static_cast<std::uint32_t>(width) * 4,
                                             .rows_per_image = static_cast<std::uint32_t>(height)};
    const granit::texture_write_region region{.width = static_cast<std::uint32_t>(width),
                                              .height = static_cast<std::uint32_t>(height)};
    result = texture.write(premultiplied_pixels, layout, region);
  }
  if (granit::succeeded(result))
    result = view.initialize(renderer, texture.native_handle());
  if (granit::succeeded(result)) {
    result = sampler.initialize(renderer, {.address_u = granit::address_mode::clamp_to_edge,
                                           .address_v = granit::address_mode::clamp_to_edge,
                                           .address_w = granit::address_mode::clamp_to_edge});
  }
  if (granit::succeeded(result)) {
    ImGui::GetIO().Fonts->SetTexID(font_texture_id);
    ImGui::GetIO().Fonts->TexRef._TexData->SetStatus(ImTextureStatus_OK);
  }
  return result;
}

bool needs_srgb_encoding(granit::texture_format format) {
  return format == granit::texture_format::rgba8_unorm ||
         format == granit::texture_format::bgra8_unorm;
}

granit::result render_frame(granit::swapchain& swapchain, granit::frame_context& frame_context,
                            std::array<bool, GRANIT_MAX_FRAMES_IN_FLIGHT>& timestamp_valid,
                            granit::timestamp_query_pool& timestamps,
                            granit::canvas_draw_list& canvas, const granit::swapchain_info& info,
                            bool timestamps_enabled, bool& needs_recreate, double& gpu_ms,
                            double& slot_wait_ms, double& acquire_ms, double& canvas_record_ms,
                            double& submit_ms, double& present_ms) {
  granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  auto result = canvas.get_stats(stats);
  if (granit::failed(result))
    return result;
  const char* operation = "acquire";
  granit::acquired_frame frame;
  const auto acquire_begin = std::chrono::steady_clock::now();
  result = swapchain.acquire(frame);
  smooth(acquire_ms,
         std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - acquire_begin)
             .count());
  if (granit::failed(result))
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  operation = "backbuffer";
  result = swapchain.backbuffer(frame.image_index, texture, view);
  granit::frame_recording recording;
  if (granit::succeeded(result)) {
    operation = "frame_context.begin";
    const auto slot_begin = std::chrono::steady_clock::now();
    result = frame_context.begin(frame, recording);
    smooth(slot_wait_ms,
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - slot_begin)
               .count());
  }
  const auto slot_index = recording.frame_slot();
  if (granit::succeeded(result) && timestamps_enabled && timestamp_valid[slot_index]) {
    std::array<std::uint64_t, 2> gpu_timestamps{};
    operation = "timestamps.results";
    result = timestamps.get_results(slot_index * 2, gpu_timestamps);
    if (granit::succeeded(result)) {
      smooth(gpu_ms, static_cast<double>(gpu_timestamps[1] - gpu_timestamps[0]) / 1'000'000.0);
    }
  }
  auto& recorder = recording.recorder();
  const auto first_query = slot_index * 2;
  if (granit::succeeded(result) && timestamps_enabled) {
    operation = "timestamps.reset";
    result = recorder.reset_timestamp_queries(timestamps.native_handle(), first_query, 2);
  }
  if (granit::succeeded(result) && timestamps_enabled) {
    operation = "timestamps.begin";
    result = recorder.write_timestamp(timestamps.native_handle(), GRANIT_TIMESTAMP_STAGE_TOP,
                                      first_query);
  }
  if (granit::succeeded(result) && stats.item_count == 0) {
    const granit::color_attachment_desc color{.view = view};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                           .area = {0, 0, info.width, info.height}};
    operation = "recorder.begin_rendering";
    result = recorder.begin_rendering(rendering);
    if (granit::succeeded(result)) {
      operation = "recorder.end_rendering";
      result = recorder.end_rendering();
    }
  } else if (granit::succeeded(result)) {
    granit_canvas_record_desc record = GRANIT_CANVAS_RECORD_DESC_INIT;
    record.color = view;
    record.color_format = static_cast<granit_texture_format>(info.format);
    record.width = info.width;
    record.height = info.height;
    record.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR;
    record.encode_srgb = needs_srgb_encoding(info.format) ? 1U : 0U;
    record.frame_slot = slot_index;
    operation = "canvas.record";
    const auto canvas_begin = std::chrono::steady_clock::now();
    result = canvas.record(recorder.native_handle(), record);
    smooth(canvas_record_ms, std::chrono::duration<double, std::milli>(
                                 std::chrono::steady_clock::now() - canvas_begin)
                                 .count());
  }
  if (granit::succeeded(result) && timestamps_enabled) {
    operation = "timestamps.end";
    result = recorder.write_timestamp(timestamps.native_handle(), GRANIT_TIMESTAMP_STAGE_BOTTOM,
                                      first_query + 1);
  }
  if (granit::succeeded(result)) {
    operation = "frame_context.submit";
    const auto submit_begin = std::chrono::steady_clock::now();
    result = recording.submit();
    smooth(submit_ms, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                submit_begin)
                          .count());
    if (granit::succeeded(result) && timestamps_enabled)
      timestamp_valid[slot_index] = true;
  }
  if (granit::succeeded(result)) {
    operation = "swapchain.present";
    const auto present_begin = std::chrono::steady_clock::now();
    result = swapchain.present(frame);
    present_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - present_begin)
            .count();
  }
  needs_recreate = needs_recreate || frame.needs_recreate;
  if (granit::failed(result)) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(swapchain.cancel(frame));
    std::cerr << "帧阶段失败：" << operation << '\n';
  }
  return result;
}

} // namespace

int main(int argc, char** argv) {
  bool smoke_test = false;
  std::uint32_t frame_limit = 0;
  std::uint32_t frame_slot_count = default_frame_slot_count;
  bool validation_enabled = true;
  bool demo_enabled = true;
  bool custom_texture_enabled = true;
  bool timestamps_enabled = true;
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--smoke-test") == 0) {
      smoke_test = true;
    } else if (std::strcmp(argv[index], "--frame-count") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const auto value = std::strtoul(argv[++index], &end, 10);
      if (end == argv[index] || *end != '\0' || value == 0 || value > UINT32_MAX)
        return 1;
      frame_limit = static_cast<std::uint32_t>(value);
    } else if (std::strcmp(argv[index], "--frames-in-flight") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const auto value = std::strtoul(argv[++index], &end, 10);
      if (end == argv[index] || *end != '\0' || value == 0 || value > GRANIT_MAX_FRAMES_IN_FLIGHT) {
        return 1;
      }
      frame_slot_count = static_cast<std::uint32_t>(value);
    } else if (std::strcmp(argv[index], "--no-validation") == 0) {
      validation_enabled = false;
    } else if (std::strcmp(argv[index], "--no-demo") == 0) {
      demo_enabled = false;
    } else if (std::strcmp(argv[index], "--no-custom-texture") == 0) {
      custom_texture_enabled = false;
    } else if (std::strcmp(argv[index], "--no-gpu-timestamps") == 0) {
      timestamps_enabled = false;
    } else {
      return 1;
    }
  }
  if (smoke_test)
    frame_limit = frame_slot_count + 1;
  if (!SDL_Init(SDL_INIT_VIDEO))
    return 1;
  sdl_quit quit;
  std::unique_ptr<SDL_Window, window_deleter> window(SDL_CreateWindow(
      "Granit SDL3 + ImGui", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
  if (!window)
    return 1;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  if (!ImGui_ImplSDL3_InitForVulkan(window.get())) {
    ImGui::DestroyContext();
    return 1;
  }
  imgui_quit imgui;
  apply_granit_theme();
  ImGui::GetIO().IniFilename = nullptr;

  granit::surface_type surface_type{};
  auto result = granit::integration::sdl3::query_surface_type(window.get(), surface_type);
  granit::renderer renderer;
  if (granit::succeeded(result)) {
    result =
        renderer.initialize({.application_name = "Granit SDL3 ImGui",
                             .enable_validation = validation_enabled,
                             .surface_types = surface_type,
                             .frames_in_flight = static_cast<std::uint32_t>(frame_slot_count)});
  }
  granit::surface surface;
  if (granit::succeeded(result)) {
    result =
        granit::integration::sdl3::create_surface(renderer.native_handle(), window.get(), surface);
  }

  int pixel_width = 0;
  int pixel_height = 0;
  if (granit::succeeded(result) &&
      !SDL_GetWindowSizeInPixels(window.get(), &pixel_width, &pixel_height)) {
    result = granit::result::backend_unavailable;
  }
  granit::swapchain swapchain;
  if (granit::succeeded(result)) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height),
                                   .presentation = granit::present_mode::immediate});
  }
  granit::swapchain_info swapchain_info;
  if (granit::succeeded(result))
    result = swapchain.query_info(swapchain_info);
  granit::frame_context frame_context;
  if (granit::succeeded(result))
    result = frame_context.initialize(renderer.native_handle());
  granit::timestamp_query_pool timestamps;
  if (granit::succeeded(result) && timestamps_enabled) {
    result = timestamps.initialize(renderer.native_handle(),
                                   static_cast<std::uint32_t>(frame_slot_count * 2));
  }
  granit::canvas_draw_list canvas;
  granit_canvas_draw_list_desc canvas_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  canvas_desc.frame_slot_count = static_cast<std::uint32_t>(frame_slot_count);
  if (granit::succeeded(result))
    result = canvas.initialize(renderer.native_handle(), canvas_desc);

  granit::texture font_texture;
  granit::texture_view font_view;
  granit::texture checker_texture;
  granit::texture_view checker_view;
  granit::sampler font_sampler;
  if (granit::succeeded(result)) {
    result = upload_font_atlas(renderer.native_handle(), font_texture, font_view, font_sampler);
  }
  if (granit::succeeded(result))
    result = upload_checker_texture(renderer.native_handle(), checker_texture, checker_view);
  if (granit::failed(result))
    std::cerr << "SDL3 + ImGui 初始化失败，Granit 结果码：" << static_cast<int>(result) << '\n';
  texture_bindings bindings{
      .font = {font_view.native_handle(), font_sampler.native_handle()},
      .checker = {checker_view.native_handle(), font_sampler.native_handle()}};

  bool running = granit::succeeded(result);
  bool recreate = false;
  bool show_demo_window = demo_enabled;
  bool validation_overlay = true;
  float render_scale = 1;
  std::uint64_t last_title_update = 0;
  std::uint32_t rendered_frames = 0;
  std::array<bool, GRANIT_MAX_FRAMES_IN_FLIGHT> timestamp_valid{};
  frame_timings timings;
  while (running) {
    const auto cpu_begin = std::chrono::steady_clock::now();
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        running = false;
      } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        pixel_width = event.window.data1;
        pixel_height = event.window.data2;
        recreate = true;
      }
    }
    if (!running)
      break;
    if (pixel_width <= 0 || pixel_height <= 0)
      continue;
    if (recreate) {
      result = swapchain.recreate({.width = static_cast<std::uint32_t>(pixel_width),
                                   .height = static_cast<std::uint32_t>(pixel_height),
                                   .presentation = granit::present_mode::immediate});
      if (result == granit::result::not_ready)
        continue;
      if (granit::failed(result) || granit::failed(result = swapchain.query_info(swapchain_info))) {
        break;
      }
      recreate = false;
    }

    const auto imgui_begin = std::chrono::steady_clock::now();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    constexpr auto panel_flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("Granit Integration", nullptr, panel_flags);
    ImGui::TextUnformatted("SDL3 owns the window and input; Granit Canvas renders ImGui.");
    ImGui::Text("Framebuffer: %u x %u", swapchain_info.width, swapchain_info.height);
    ImGui::Text("Presentation: %s", swapchain_info.presentation == granit::present_mode::immediate
                                        ? "Immediate"
                                        : "FIFO fallback");
    ImGui::Text("CPU %.3f ms | GPU %.3f ms | Present %.3f ms | Slot wait %.3f ms", timings.cpu_ms,
                timings.gpu_ms, timings.present_ms, timings.slot_wait_ms);
    ImGui::Separator();
    ImGui::Checkbox("Show ImGui demo", &show_demo_window);
    ImGui::Checkbox("Validation overlay", &validation_overlay);
    ImGui::SliderFloat("Render scale", &render_scale, 0.5F, 2, "%.2fx");
    if (custom_texture_enabled) {
      ImGui::TextUnformatted("Custom Texture ID:");
      ImGui::Image(ImTextureRef{checker_texture_id}, {64, 64});
    }
    if (ImGui::Button("Reload shaders"))
      render_scale = 1;
    ImGui::SameLine();
    ImGui::TextDisabled("Modern Granit dark theme");
    ImGui::End();
    if (show_demo_window)
      ImGui::ShowDemoWindow(&show_demo_window);
    ImGui::Render();
    smooth(timings.imgui_ms,
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - imgui_begin)
               .count());
    const auto now = SDL_GetTicks();
    if (now - last_title_update >= 500) {
      char title[160]{};
      std::snprintf(title, sizeof(title),
                    "Granit SDL3 + ImGui | %.0f FPS | CPU %.2f | GPU %.2f | Present %.2f | Slot "
                    "%.2f ms",
                    ImGui::GetIO().Framerate, timings.cpu_ms, timings.gpu_ms, timings.present_ms,
                    timings.slot_wait_ms);
      SDL_SetWindowTitle(window.get(), title);
      last_title_update = now;
    }
    const auto convert_begin = std::chrono::steady_clock::now();
    result = canvas.clear();
    if (granit::succeeded(result)) {
      result = granit::integration::imgui::append_draw_data(ImGui::GetDrawData(), canvas,
                                                            resolve_texture, &bindings);
      if (granit::failed(result))
        std::cerr << "ImGui Draw Data 转换失败，Granit 结果码：" << static_cast<int>(result)
                  << '\n';
    }
    smooth(timings.convert_ms, std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - convert_begin)
                                   .count());
    if (granit::succeeded(result)) {
      const auto render_begin = std::chrono::steady_clock::now();
      double present_ms = 0;
      result = render_frame(swapchain, frame_context, timestamp_valid, timestamps, canvas,
                            swapchain_info, timestamps_enabled, recreate, timings.gpu_ms,
                            timings.slot_wait_ms, timings.acquire_ms, timings.canvas_record_ms,
                            timings.submit_ms, present_ms);
      smooth(timings.render_ms, std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - render_begin)
                                    .count());
      smooth(timings.present_ms, present_ms);
      if (granit::failed(result) && result != granit::result::out_of_date)
        std::cerr << "ImGui Canvas 录制或呈现失败，Granit 结果码：" << static_cast<int>(result)
                  << '\n';
    }
    if (result == granit::result::out_of_date) {
      result = granit::result::success;
      recreate = true;
      continue;
    }
    if (granit::failed(result))
      break;
    smooth(timings.cpu_ms,
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpu_begin)
               .count());
    ++rendered_frames;
    if (frame_limit != 0 && rendered_frames >= frame_limit)
      break;
  }

  if (frame_limit != 0 && granit::succeeded(result)) {
    std::cout << "完成 " << rendered_frames << " 帧（槽数 " << frame_slot_count << "）：CPU "
              << timings.cpu_ms << " ms，ImGui " << timings.imgui_ms << " ms，转换 "
              << timings.convert_ms << " ms，渲染 " << timings.render_ms << " ms，GPU "
              << timings.gpu_ms << " ms，Present " << timings.present_ms << " ms，槽等待 "
              << timings.slot_wait_ms << " ms，Acquire " << timings.acquire_ms
              << " ms，Canvas Record " << timings.canvas_record_ms << " ms，Submit "
              << timings.submit_ms << " ms\n";
  }

  if (granit::failed(result))
    std::cerr << "SDL3 + ImGui 帧循环失败，Granit 结果码：" << static_cast<int>(result) << '\n';
  return granit::failed(result) ? 1 : 0;
}
