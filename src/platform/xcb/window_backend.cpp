// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/window/window_backend_internal.h"

#include <cstdlib>
#include <cstring>
#include <memory>

namespace granit::window::detail {

xcb_atom_t intern_xcb_atom(xcb_connection_t* connection, const char* name) {
  const auto cookie =
      xcb_intern_atom(connection, 0, static_cast<std::uint16_t>(std::strlen(name)), name);
  std::unique_ptr<xcb_intern_atom_reply_t, decltype(&std::free)> reply{
      xcb_intern_atom_reply(connection, cookie, nullptr), &std::free};
  return reply ? reply->atom : static_cast<xcb_atom_t>(XCB_ATOM_NONE);
}

void pump_xcb_events(const std::shared_ptr<window_system_record>& system) {
  const auto public_handle = [&](xcb_window_t native_window) {
    for (const auto& [handle, window] : system->windows)
      if (window->window == native_window)
        return handle;
    return GRANIT_NULL_HANDLE;
  };
  while (auto* native_event = xcb_poll_for_event(system->connection)) {
    std::unique_ptr<xcb_generic_event_t, decltype(&std::free)> event{native_event, &std::free};
    const auto type = static_cast<std::uint8_t>(event->response_type & UINT8_C(0x7f));
    const auto dispatch_input = [&](granit_window window, std::int16_t x, std::int16_t y,
                                    std::uint16_t state, std::uint8_t detail) {
      if (system->input_native_event == nullptr || window == GRANIT_NULL_HANDLE)
        return;
      const granit_window_input_native_event input_event{
          GRANIT_WINDOW_INPUT_BACKEND_XCB, type, 0, 0, x, y, state, detail, 0, 0};
      system->input_native_event(system->input_user_data, window, &input_event);
    };
    if (type == XCB_KEY_PRESS || type == XCB_KEY_RELEASE) {
      const auto* key = reinterpret_cast<const xcb_key_press_event_t*>(event.get());
      dispatch_input(public_handle(key->event), key->event_x, key->event_y, key->state,
                     key->detail);
    } else if (type == XCB_BUTTON_PRESS || type == XCB_BUTTON_RELEASE) {
      const auto* button = reinterpret_cast<const xcb_button_press_event_t*>(event.get());
      dispatch_input(public_handle(button->event), button->event_x, button->event_y, button->state,
                     button->detail);
    } else if (type == XCB_MOTION_NOTIFY) {
      const auto* motion = reinterpret_cast<const xcb_motion_notify_event_t*>(event.get());
      dispatch_input(public_handle(motion->event), motion->event_x, motion->event_y, motion->state,
                     motion->detail);
    } else if (type == XCB_ENTER_NOTIFY || type == XCB_LEAVE_NOTIFY) {
      const auto* crossing = reinterpret_cast<const xcb_enter_notify_event_t*>(event.get());
      dispatch_input(public_handle(crossing->event), crossing->event_x, crossing->event_y,
                     crossing->state, crossing->detail);
    }
    if (type == XCB_CONFIGURE_NOTIFY) {
      const auto* configured = reinterpret_cast<const xcb_configure_notify_event_t*>(event.get());
      granit_window_event output = GRANIT_WINDOW_EVENT_INIT;
      output.type = GRANIT_WINDOW_EVENT_RESIZED;
      output.window = public_handle(configured->window);
      output.timestamp_ns = timestamp_ns();
      output.data.resized.width = configured->width;
      output.data.resized.height = configured->height;
      system->events.push_back(output);
    } else if (type == XCB_FOCUS_IN || type == XCB_FOCUS_OUT) {
      const auto* focused = reinterpret_cast<const xcb_focus_in_event_t*>(event.get());
      granit_window_event output = GRANIT_WINDOW_EVENT_INIT;
      output.type = GRANIT_WINDOW_EVENT_FOCUS_CHANGED;
      output.window = public_handle(focused->event);
      output.timestamp_ns = timestamp_ns();
      output.data.focus.focused = type == XCB_FOCUS_IN ? UINT32_C(1) : UINT32_C(0);
      system->events.push_back(output);
      if (type == XCB_FOCUS_OUT && system->input_focus_lost != nullptr)
        system->input_focus_lost(system->input_user_data, output.window);
    } else if (type == XCB_CLIENT_MESSAGE) {
      const auto* message = reinterpret_cast<const xcb_client_message_event_t*>(event.get());
      if (message->type == system->wm_protocols &&
          message->data.data32[0] == system->wm_delete_window)
        enqueue_event(system, public_handle(message->window), GRANIT_WINDOW_EVENT_CLOSE_REQUESTED);
    }
  }
}

} // namespace granit::window::detail
