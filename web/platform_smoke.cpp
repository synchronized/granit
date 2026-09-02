// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <granit/pipeline/render_pipeline.h>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/renderer.h>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/surface.h>
#include <granit/renderer/swapchain.h>
#include <granit/renderer/texture.hpp>

#include "model_viewer/application_core.h"
#include "model_viewer_fetch.h"
#include "resource_fetch_batch.h"
#include "support/renderer_fixture.h"
#include "web_input.h"

namespace {

enum class startup_status : int { failed = -1, starting, provider_pending, ready };

struct web_platform_state {
  granit_renderer renderer{};
  granit_surface surface{};
  granit_swapchain swapchain{};
  granit_render_pipeline pipeline{};
  startup_status status{startup_status::starting};
  unsigned input_event_count{};
  unsigned applied_input_count{};
  unsigned rendered_frame_count{};
  unsigned resize_count{};
  granit::example::model_viewer::web::web_input input;
  std::shared_ptr<granit::example::model_viewer::web::asset_request> asset_request{
      std::make_shared<granit::example::model_viewer::web::asset_request>()};
  granit::example::model_viewer::web::resource_fetch_batch resource_batch;
  granit::example::model_viewer::web::resource_bundle resource_bundle;
  granit::example::model_viewer::application_core core;
  bool core_renderer_ready{};
  bool resource_batch_started{};
  bool asset_ready{};
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

std::string load_text_resource(const char* path) {
  auto* file = std::fopen(path, "rb");
  if (file == nullptr)
    return {};
  std::string content;
  std::array<char, 1024> buffer{};
  while (const auto size = std::fread(buffer.data(), 1, buffer.size(), file))
    content.append(buffer.data(), size);
  std::fclose(file);
  return content;
}

bool validate_fixture_assets() {
  const auto vertex = load_text_resource("/assets/dynamic_uniform.vert.wgsl");
  const auto fragment = load_text_resource("/assets/dynamic_uniform.frag.wgsl");
  return !vertex.empty() && !fragment.empty() &&
         granit::test::renderer_fixture::vertices.size() == 4 * 7 &&
         granit::test::renderer_fixture::indices.size() == 6 &&
         granit::test::renderer_fixture::make_uniform_data().size() == 4 * 256;
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

granit_result draw_shared_fixture(granit_frame frame, granit_texture_view target_view,
                                  granit_texture_format native_format, std::uint32_t width,
                                  std::uint32_t height) {
  const auto vertex_wgsl = load_text_resource("/assets/dynamic_uniform.vert.wgsl");
  const auto fragment_wgsl = load_text_resource("/assets/dynamic_uniform.frag.wgsl");
  granit::shader vertex;
  granit::shader fragment;
  auto result = vertex.initialize_asset(
      state.renderer, {.stage = granit::shader_stage::vertex, .spirv = {}, .wgsl = vertex_wgsl});
  if (result != granit::result::success)
    return granit::to_native(result);
  result = fragment.initialize_asset(
      state.renderer,
      {.stage = granit::shader_stage::fragment, .spirv = {}, .wgsl = fragment_wgsl});
  if (result != granit::result::success)
    return granit::to_native(result);

  const std::array declarations{
      granit::bind_group_layout_entry{.binding = 0,
                                      .type = granit::binding_type::dynamic_uniform_buffer,
                                      .visibility = granit::shader_stage_flags::vertex},
      granit::bind_group_layout_entry{.binding = 1,
                                      .type = granit::binding_type::sampled_texture,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = 2,
                                      .type = granit::binding_type::sampler,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = 3,
                                      .type = granit::binding_type::sampled_texture,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = 4,
                                      .type = granit::binding_type::sampled_texture,
                                      .visibility = granit::shader_stage_flags::fragment}};
  granit::bind_group_layout group_layout;
  result = group_layout.initialize(state.renderer, declarations);
  if (result != granit::result::success)
    return granit::to_native(result);
  const auto group_layout_handle = group_layout.native_handle();
  granit::pipeline_layout pipeline_layout;
  result = pipeline_layout.initialize(state.renderer, std::span{&group_layout_handle, 1});
  if (result != granit::result::success)
    return granit::to_native(result);

  const auto color_format = static_cast<granit::texture_format>(native_format);
  const std::array vertex_attributes{
      granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x2},
      granit::vertex_attribute{
          .location = 1, .format = granit::vertex_format::float32x2, .offset = sizeof(float) * 2},
      granit::vertex_attribute{
          .location = 2, .format = granit::vertex_format::float32x3, .offset = sizeof(float) * 4}};
  const std::array vertex_layouts{
      granit::vertex_buffer_layout{.stride = sizeof(float) * 7, .attributes = vertex_attributes}};
  granit::graphics_pipeline pipeline;
  result = pipeline.initialize(
      state.renderer, {.layout = pipeline_layout.native_handle(),
                       .vertex_shader = vertex.native_handle(),
                       .fragment_shader = fragment.native_handle(),
                       .color_formats = std::span{&color_format, 1},
                       .depth_stencil_format = granit::texture_format::d32_float,
                       .vertex_buffers = vertex_layouts,
                       .primitive = {},
                       .depth = granit::depth_state{.test_enabled = true, .write_enabled = true},
                       .color_blends = {},
                       .depth_bias = std::nullopt});
  if (result != granit::result::success)
    return granit::to_native(result);

  const auto uniform_data = granit::test::renderer_fixture::make_uniform_data();
  granit::buffer uniform;
  result = uniform.initialize(
      state.renderer,
      {.size = uniform_data.size(),
       .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination},
      uniform_data);
  if (result != granit::result::success)
    return granit::to_native(result);

  const granit::texture_desc base_desc{.format = granit::texture_format::rgba8_srgb,
                                       .usage = granit::texture_usage::sampled |
                                                granit::texture_usage::transfer_destination,
                                       .width = 2,
                                       .height = 2};
  const granit::texture_write_region texture_region{.array_layer_count = 1,
                                                    .aspect = granit::texture_aspect::color,
                                                    .width = 2,
                                                    .height = 2,
                                                    .depth = 1};
  granit::texture base_color;
  granit::texture_view base_color_view;
  result = base_color.initialize(state.renderer, base_desc);
  if (result == granit::result::success)
    result = base_color_view.initialize(state.renderer, base_color.native_handle());
  if (result == granit::result::success)
    result = base_color.write(
        std::as_bytes(std::span{granit::test::renderer_fixture::base_color_pixels}), {},
        texture_region);
  granit::texture normal;
  granit::texture_view normal_view;
  auto material_desc = base_desc;
  material_desc.format = granit::texture_format::rgba8_unorm;
  if (result == granit::result::success)
    result = normal.initialize(state.renderer, material_desc);
  if (result == granit::result::success)
    result = normal_view.initialize(state.renderer, normal.native_handle());
  if (result == granit::result::success)
    result = normal.write(std::as_bytes(std::span{granit::test::renderer_fixture::normal_pixels}),
                          {}, texture_region);
  granit::texture metallic_roughness;
  granit::texture_view metallic_roughness_view;
  if (result == granit::result::success)
    result = metallic_roughness.initialize(state.renderer, material_desc);
  if (result == granit::result::success)
    result = metallic_roughness_view.initialize(state.renderer, metallic_roughness.native_handle());
  if (result == granit::result::success)
    result = metallic_roughness.write(
        std::as_bytes(std::span{granit::test::renderer_fixture::metallic_roughness_pixels}), {},
        texture_region);
  if (result != granit::result::success)
    return granit::to_native(result);

  granit::sampler material_sampler;
  result = material_sampler.initialize(state.renderer);
  if (result != granit::result::success)
    return granit::to_native(result);
  const std::array entries{
      granit::bind_group_entry{
          .binding = 0, .resource = uniform.native_handle(), .offset = 0, .size = 32},
      granit::bind_group_entry{.binding = 1, .resource = base_color_view.native_handle()},
      granit::bind_group_entry{.binding = 2, .resource = material_sampler.native_handle()},
      granit::bind_group_entry{.binding = 3, .resource = normal_view.native_handle()},
      granit::bind_group_entry{.binding = 4, .resource = metallic_roughness_view.native_handle()}};
  granit::bind_group group;
  result = group.initialize(state.renderer, group_layout.native_handle(), entries);
  if (result != granit::result::success)
    return granit::to_native(result);

  constexpr auto& vertices = granit::test::renderer_fixture::vertices;
  constexpr auto& indices = granit::test::renderer_fixture::indices;
  granit::buffer vertex_buffer;
  granit::buffer index_buffer;
  result = vertex_buffer.initialize(
      state.renderer,
      {.size = sizeof(vertices),
       .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination},
      std::as_bytes(std::span{vertices}));
  if (result == granit::result::success)
    result = index_buffer.initialize(
        state.renderer,
        {.size = sizeof(indices),
         .usage = granit::buffer_usage::index | granit::buffer_usage::transfer_destination},
        std::as_bytes(std::span{indices}));

  granit::texture depth_target;
  granit::texture_view depth_view;
  if (result == granit::result::success)
    result = depth_target.initialize(state.renderer,
                                     {.format = granit::texture_format::d32_float,
                                      .usage = granit::texture_usage::depth_stencil_attachment,
                                      .width = width,
                                      .height = height});
  if (result == granit::result::success)
    result = depth_view.initialize(state.renderer, depth_target.native_handle());
  if (result != granit::result::success)
    return granit::to_native(result);

  granit_command_recorder recorder{};
  const granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  auto native_result = granit_command_recorder_create(state.renderer, &recorder_desc, &recorder);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_begin(state.renderer, recorder);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_bind_graphics_pipeline(state.renderer, recorder,
                                                                   pipeline.native_handle());
  const granit_viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
  const granit_scissor scissor{0, 0, width, height};
  if (native_result == GRANIT_SUCCESS)
    native_result =
        granit_command_recorder_set_viewports(state.renderer, recorder, 0, &viewport, 1);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_set_scissors(state.renderer, recorder, 0, &scissor, 1);
  const granit_vertex_buffer_binding vertex_binding{vertex_buffer.native_handle(), 0};
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_bind_vertex_buffers(state.renderer, recorder, 0,
                                                                &vertex_binding, 1);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_bind_index_buffer(
        state.renderer, recorder, index_buffer.native_handle(), 0, GRANIT_INDEX_TYPE_UINT16);
  const granit_bind_group group_handle = group.native_handle();
  const std::array offsets{UINT32_C(0), UINT32_C(512), UINT32_C(768), UINT32_C(256)};
  granit_bind_groups_desc groups = GRANIT_BIND_GROUPS_DESC_INIT;
  groups.first_group = 0;
  groups.bind_group_count = 1;
  groups.bind_groups = &group_handle;
  groups.dynamic_offset_count = 1;
  groups.dynamic_offsets = offsets.data();
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_bind_graphics_groups(
        state.renderer, recorder, pipeline_layout.native_handle(), &groups);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = target_view;
  granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth.view = depth_view.native_handle();
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.depth_stencil_attachment = &depth;
  rendering.area = {0, 0, width, height};
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_begin_rendering(state.renderer, recorder, &rendering);
  for (std::size_t index = 0; native_result == GRANIT_SUCCESS && index < offsets.size(); ++index) {
    if (index != 0) {
      groups.dynamic_offsets = offsets.data() + index;
      native_result = granit_command_recorder_bind_graphics_groups(
          state.renderer, recorder, pipeline_layout.native_handle(), &groups);
    }
    if (native_result == GRANIT_SUCCESS)
      native_result = granit_command_recorder_draw_indexed(
          state.renderer, recorder, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
  }
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_end_rendering(state.renderer, recorder);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_end(state.renderer, recorder);
  if (native_result == GRANIT_SUCCESS)
    native_result = granit_command_recorder_submit_frame(state.renderer, recorder, frame);
  static_cast<void>(granit_command_recorder_destroy(state.renderer, recorder));
  return native_result;
}

void diagnose(granit_diagnostic_severity, granit_diagnostic_category, const char* message,
              std::uint32_t message_length, void*) noexcept {
  std::fprintf(stderr, "GRANIT_DIAGNOSTIC:%.*s\n", static_cast<int>(message_length), message);
}

EM_BOOL receive_keyboard(int event_type, const EmscriptenKeyboardEvent* event,
                         void* user_data) noexcept {
  auto& platform = *static_cast<web_platform_state*>(user_data);
  ++platform.input_event_count;
  if (event_type == EMSCRIPTEN_EVENT_KEYDOWN) {
    using granit::example::model_viewer::web::shortcut_key;
    auto key = shortcut_key::other;
    if (std::strcmp(event->key, "f") == 0 || std::strcmp(event->key, "F") == 0)
      key = shortcut_key::focus;
    else if (std::strcmp(event->key, "Home") == 0)
      key = shortcut_key::home;
    platform.input.key_pressed(key, event->repeat != 0);
  }
  return EM_FALSE;
}

EM_BOOL receive_mouse(int event_type, const EmscriptenMouseEvent* event,
                      void* user_data) noexcept {
  auto& platform = *static_cast<web_platform_state*>(user_data);
  ++platform.input_event_count;
  using granit::example::model_viewer::web::pointer_button;
  if (event_type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
    platform.input.pointer_motion(static_cast<float>(event->movementX),
                                  static_cast<float>(event->movementY));
  } else if (event_type == EMSCRIPTEN_EVENT_MOUSEDOWN ||
             event_type == EMSCRIPTEN_EVENT_MOUSEUP) {
    auto button = pointer_button::primary;
    if (event->button == 1)
      button = pointer_button::middle;
    else if (event->button == 2)
      button = pointer_button::secondary;
    platform.input.pointer_button_changed(button, event_type == EMSCRIPTEN_EVENT_MOUSEDOWN);
  } else if (event_type == EMSCRIPTEN_EVENT_MOUSEENTER) {
    platform.input.pointer_presence_changed(true);
  } else if (event_type == EMSCRIPTEN_EVENT_MOUSELEAVE) {
    platform.input.pointer_presence_changed(false);
  }
  return EM_FALSE;
}

EM_BOOL receive_wheel(int, const EmscriptenWheelEvent* event, void* user_data) noexcept {
  auto& platform = *static_cast<web_platform_state*>(user_data);
  ++platform.input_event_count;
  platform.input.wheel(static_cast<float>(event->deltaY));
  return EM_TRUE;
}

EM_BOOL receive_focus(int event_type, const EmscriptenFocusEvent*, void* user_data) noexcept {
  auto& platform = *static_cast<web_platform_state*>(user_data);
  ++platform.input_event_count;
  platform.input.focus_changed(event_type == EMSCRIPTEN_EVENT_FOCUS);
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
    result = draw_shared_fixture(frame, view, info.format, info.width, info.height);
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

granit_result resize_swapchain_if_needed() {
  int width{};
  int height{};
  if (emscripten_get_canvas_element_size("#canvas", &width, &height) != EMSCRIPTEN_RESULT_SUCCESS ||
      width <= 0 || height <= 0) {
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;
  auto result = granit_swapchain_get_info(state.renderer, state.swapchain, &info);
  if (result != GRANIT_SUCCESS ||
      (info.width == static_cast<std::uint32_t>(width) &&
       info.height == static_cast<std::uint32_t>(height))) {
    return result;
  }
  result = granit_swapchain_destroy(state.renderer, state.swapchain);
  if (result != GRANIT_SUCCESS)
    return result;
  state.swapchain = GRANIT_NULL_HANDLE;
  granit_swapchain_desc desc = GRANIT_SWAPCHAIN_DESC_INIT;
  desc.width = static_cast<std::uint32_t>(width);
  desc.height = static_cast<std::uint32_t>(height);
  desc.minimum_image_count = 2;
  result = granit_swapchain_create(state.renderer, state.surface, &desc, &state.swapchain);
  if (result == GRANIT_SUCCESS)
    ++state.resize_count;
  return result;
}

granit_result render_model_viewer_frame() {
  auto result = resize_swapchain_if_needed();
  if (result != GRANIT_SUCCESS)
    return result;
  granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;
  result = granit_swapchain_get_info(state.renderer, state.swapchain, &info);
  granit_frame frame{};
  std::uint32_t image_index{};
  std::uint32_t needs_recreate{};
  if (result == GRANIT_SUCCESS)
    result = granit_swapchain_acquire(state.renderer, state.swapchain, &frame, &image_index,
                                      &needs_recreate);
  granit_texture backbuffer{};
  granit_texture_view backbuffer_view{};
  if (result == GRANIT_SUCCESS) {
    result = granit_swapchain_get_backbuffer(state.renderer, state.swapchain, image_index,
                                             &backbuffer, &backbuffer_view);
  }
  granit::example::model_viewer::application_tick_output output;
  granit::example::model_viewer::application_tick_input input;
  input.input = state.input.finish(false, false);
  if (input.input.pointer_delta_x != 0.0F || input.input.pointer_delta_y != 0.0F ||
      input.input.wheel_delta != 0.0F || input.input.focus_requested || input.input.home_requested)
    ++state.applied_input_count;
  state.input.begin_frame();
  input.width = info.width;
  input.height = info.height;
  if (result == GRANIT_SUCCESS)
    result = granit::to_native(state.core.tick(input, output));
  if (result == GRANIT_SUCCESS) {
    output.render.output = backbuffer_view;
    output.render.output_format = info.format;
    output.render.frame = frame;
    result = granit_render_pipeline_render(state.renderer, state.pipeline, &output.render);
  }
  if (result != GRANIT_SUCCESS) {
    if (frame != GRANIT_NULL_HANDLE)
      static_cast<void>(
          granit_frame_cancel(state.renderer, state.swapchain, frame, &needs_recreate));
    return result;
  }
  result = granit_swapchain_present(state.renderer, state.swapchain, frame, &needs_recreate);
  if (result == GRANIT_SUCCESS)
    ++state.rendered_frame_count;
  return result;
}

void tick(void*) noexcept {
  if (state.status == startup_status::failed) {
    return;
  }
  if (state.status == startup_status::ready) {
    const auto result = render_model_viewer_frame();
    if (result != GRANIT_SUCCESS)
      fail("model-viewer-frame", result);
    return;
  }
  if (state.status != startup_status::provider_pending)
    return;
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
  if (!state.core_renderer_ready) {
    const auto result = state.core.renderer_ready();
    if (result != granit::result::success) {
      fail("core-renderer-ready", granit::to_native(result));
      return;
    }
    state.core_renderer_ready = true;
  }
  if (state.asset_request->status() ==
      granit::example::model_viewer::web::asset_request_status::failed) {
    fail("asset-fetch");
    return;
  }
  if (state.asset_request->status() !=
      granit::example::model_viewer::web::asset_request_status::ready) {
    return;
  }

  try {
    if (!state.resource_batch_started) {
      std::vector<std::string> resources;
      const auto discovery = granit::example::gltf::discover_external_resources(
          state.asset_request->bytes(), resources);
      if (!discovery) {
        fail("asset-discovery", GRANIT_ERROR_INVALID_ARGUMENT);
        return;
      }
      for (const auto& resource : resources) {
        if (!state.resource_batch.add(resource, resource)) {
          fail("asset-batch-add", GRANIT_ERROR_INVALID_ARGUMENT);
          return;
        }
      }
      for (const auto& entry : state.resource_batch.entries()) {
        if (!granit::example::model_viewer::web::start_fetch(entry.request, entry.url)) {
          fail("asset-resource-fetch-start");
          return;
        }
      }
      state.resource_batch_started = true;
    }

    const auto batch_status = state.resource_batch.status();
    if (batch_status == granit::example::model_viewer::web::resource_fetch_batch_status::failed) {
      fail("asset-resource-fetch");
      return;
    }
    if (batch_status != granit::example::model_viewer::web::resource_fetch_batch_status::ready)
      return;
    if (!state.asset_ready) {
      if (!state.resource_batch.commit(state.resource_bundle)) {
        fail("asset-bundle-commit", GRANIT_ERROR_INTERNAL);
        return;
      }
      auto result = state.core.load_asset(state.asset_request->bytes(), &state.resource_bundle);
      if (result != granit::result::success) {
        fail("asset-load", granit::to_native(result));
        return;
      }
      result = state.core.upload(state.renderer);
      if (result != granit::result::success) {
        fail("asset-upload", granit::to_native(result));
        return;
      }
      if (state.core.phase() != granit::example::model_viewer::application_phase::ready) {
        fail("asset-core-phase", GRANIT_ERROR_INTERNAL);
        return;
      }
      state.asset_ready = true;
    }
  } catch (const std::bad_alloc&) {
    fail("asset-allocation", GRANIT_ERROR_OUT_OF_MEMORY);
    return;
  } catch (...) {
    fail("asset-exception", GRANIT_ERROR_INTERNAL);
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
    const granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
    const auto pipeline_result =
        granit_render_pipeline_create(state.renderer, &pipeline_desc, &state.pipeline);
    if (pipeline_result != GRANIT_SUCCESS) {
      fail("pipeline-create", pipeline_result);
      return;
    }
    const auto result = create_presentation_resources();
    if (result != GRANIT_SUCCESS) {
      static_cast<void>(granit_render_pipeline_destroy(state.renderer, state.pipeline));
      state.pipeline = GRANIT_NULL_HANDLE;
      fail("presentation-create", result);
      return;
    }
    const auto render_result = render_model_viewer_frame();
    if (render_result != GRANIT_SUCCESS) {
      static_cast<void>(granit_render_pipeline_destroy(state.renderer, state.pipeline));
      state.pipeline = GRANIT_NULL_HANDLE;
      fail("model-viewer-render", render_result);
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

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_rendered_frame_count() noexcept {
  return state.rendered_frame_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_applied_input_count() noexcept {
  return state.applied_input_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_resize_count() noexcept {
  return state.resize_count;
}

extern "C" EMSCRIPTEN_KEEPALIVE unsigned granit_web_asset_status() noexcept {
  if (state.status == startup_status::failed)
    return 3;
  return state.asset_ready ? 2U : 1U;
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
  if (!load_startup_resource() || !validate_fixture_assets()) {
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
  const auto core_result = state.core.begin_renderer();
  if (core_result != granit::result::success) {
    fail("core-renderer-begin", granit::to_native(core_result));
    return 1;
  }
  if (!granit::example::model_viewer::web::start_fetch(state.asset_request,
                                                       "model_viewer_fixture.gltf")) {
    fail("asset-fetch-start");
    return 1;
  }
  static_cast<void>(emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &state,
                                                    EM_FALSE, receive_keyboard));
  static_cast<void>(emscripten_set_mousedown_callback("#canvas", &state, EM_FALSE, receive_mouse));
  static_cast<void>(emscripten_set_mouseup_callback("#canvas", &state, EM_FALSE, receive_mouse));
  static_cast<void>(emscripten_set_mousemove_callback("#canvas", &state, EM_FALSE, receive_mouse));
  static_cast<void>(emscripten_set_mouseenter_callback("#canvas", &state, EM_FALSE, receive_mouse));
  static_cast<void>(emscripten_set_mouseleave_callback("#canvas", &state, EM_FALSE, receive_mouse));
  static_cast<void>(emscripten_set_wheel_callback("#canvas", &state, EM_FALSE, receive_wheel));
  static_cast<void>(emscripten_set_focus_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &state, EM_FALSE,
                                                 receive_focus));
  static_cast<void>(emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &state, EM_FALSE,
                                                receive_focus));
  state.input.focus_changed(true);

  state.status = startup_status::provider_pending;
  emscripten_set_main_loop_arg(tick, &state, 0, EM_FALSE);
  return 0;
}
