// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <xcb/xcb.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>

namespace {

xcb_atom_t intern_atom(xcb_connection_t* connection, const char* name) {
  const auto length = static_cast<std::uint16_t>(std::strlen(name));
  const auto cookie = xcb_intern_atom(connection, 0, length, name);
  auto* reply = xcb_intern_atom_reply(connection, cookie, nullptr);
  if (reply == nullptr)
    return XCB_ATOM_NONE;
  const auto atom = reply->atom;
  std::free(reply);
  return atom;
}

granit::result render_frame(granit::swapchain& swapchain, granit::frame_context& context,
                            std::uint32_t width, std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (result.failed())
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  result = swapchain.backbuffer(frame.image_index, texture, view);
  granit::frame_recording recording;
  if (result.ok())
    result = context.begin(frame, recording);
  auto& recorder = recording.recorder();
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.04F, .green = 0.12F, .blue = 0.22F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, width, height}};
  if (result.ok())
    result = recorder.begin_rendering(rendering);
  if (result.ok())
    result = recorder.end_rendering();
  if (result.ok())
    result = recording.submit();
  if (result.ok())
    result = swapchain.present(frame);
  needs_recreate = needs_recreate || frame.needs_recreate;
  if (result.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(swapchain.cancel(frame));
  }
  return result;
}

} // namespace

int main(int argc, char** argv) {
  const bool smoke_test = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
  int screen_number = 0;
  auto* connection = xcb_connect(nullptr, &screen_number);
  if (connection == nullptr || xcb_connection_has_error(connection) != 0) {
    if (connection != nullptr)
      xcb_disconnect(connection);
    return 1;
  }

  auto iterator = xcb_setup_roots_iterator(xcb_get_setup(connection));
  for (int index = 0; index < screen_number && iterator.rem != 0; ++index)
    xcb_screen_next(&iterator);
  if (iterator.rem == 0) {
    xcb_disconnect(connection);
    return 1;
  }
  const auto* screen = iterator.data;

  const auto window = xcb_generate_id(connection);
  const std::uint32_t values[] = {screen->black_pixel,
                                  XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE};
  xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                    XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
  constexpr char title[] = "Granit XCB 窗口清屏";
  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
                      8, sizeof(title) - 1, title);

  const auto wm_protocols = intern_atom(connection, "WM_PROTOCOLS");
  const auto wm_delete_window = intern_atom(connection, "WM_DELETE_WINDOW");
  if (wm_protocols != XCB_ATOM_NONE && wm_delete_window != XCB_ATOM_NONE) {
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, wm_protocols, XCB_ATOM_ATOM, 32,
                        1, &wm_delete_window);
  }
  xcb_map_window(connection, window);
  xcb_flush(connection);

  granit::renderer renderer;
  auto result = renderer.initialize({.application_name = "Granit XCB Window Clear",
                                     .enable_validation = true,
                                     .surface_types = granit::surface_type::xcb});
  granit::surface surface;
  if (result.ok())
    result = surface.initialize_xcb(renderer.native_handle(),
                                    {.connection = connection, .window = window});

  std::uint32_t width = 800;
  std::uint32_t height = 600;
  granit::swapchain swapchain;
  if (result.ok()) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = width, .height = height});
  }
  granit::frame_context frame_context;
  if (result.ok())
    result = frame_context.initialize(renderer.native_handle());

  bool running = result.ok();
  bool recreate = false;
  std::uint32_t rendered_frames = 0;
  while (running) {
    while (auto* event = xcb_poll_for_event(connection)) {
      const auto type = static_cast<std::uint8_t>(event->response_type & ~0x80U);
      if (type == XCB_CLIENT_MESSAGE) {
        const auto* message = reinterpret_cast<xcb_client_message_event_t*>(event);
        if (message->data.data32[0] == wm_delete_window)
          running = false;
      } else if (type == XCB_DESTROY_NOTIFY) {
        running = false;
      } else if (type == XCB_CONFIGURE_NOTIFY) {
        const auto* configure = reinterpret_cast<xcb_configure_notify_event_t*>(event);
        const auto next_width = static_cast<std::uint32_t>(configure->width);
        const auto next_height = static_cast<std::uint32_t>(configure->height);
        if (next_width != width || next_height != height) {
          width = next_width;
          height = next_height;
          recreate = true;
        }
      }
      std::free(event);
    }
    if (!running)
      break;
    if (xcb_connection_has_error(connection) != 0) {
      result = granit::result::backend_unavailable;
      break;
    }
    if (width == 0 || height == 0)
      continue;
    if (recreate) {
      result = swapchain.recreate({.width = width, .height = height});
      if (result == granit::result::not_ready)
        continue;
      if (result.failed())
        break;
      recreate = false;
    }

    result = render_frame(swapchain, frame_context, width, height, recreate);
    if (result == granit::result::out_of_date) {
      recreate = true;
      continue;
    }
    if (result.failed())
      break;
    ++rendered_frames;
    if (smoke_test && rendered_frames >= 3)
      break;
  }

  const auto reset_resource = [&result](auto& resource) {
    const auto reset_result = resource.reset();
    if (result.ok() && reset_result.failed())
      result = reset_result;
  };
  reset_resource(frame_context);
  reset_resource(swapchain);
  reset_resource(surface);
  reset_resource(renderer);
  xcb_destroy_window(connection, window);
  xcb_disconnect(connection);
  return result.failed() ? 1 : 0;
}
