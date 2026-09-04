// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/window/window_backend_internal.h"

#include <cstdlib>
#include <cstring>
#include <memory>

namespace granit::window::detail {

xcb_atom_t intern_atom(xcb_connection_t* connection, const char* name) {
  const auto cookie =
      xcb_intern_atom(connection, 0, static_cast<std::uint16_t>(std::strlen(name)), name);
  std::unique_ptr<xcb_intern_atom_reply_t, decltype(&std::free)> reply{
      xcb_intern_atom_reply(connection, cookie, nullptr), &std::free};
  return reply ? reply->atom : static_cast<xcb_atom_t>(XCB_ATOM_NONE);
}

granit_result create_xcb_system(granit_window_system* output) {
  try {
    auto system = std::make_shared<window_system_record>();
    system->connection = xcb_connect(nullptr, nullptr);
    if (system->connection == nullptr)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)> guard{system->connection,
                                                                       &xcb_disconnect};
    if (xcb_connection_has_error(system->connection) != 0)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    auto screens = xcb_setup_roots_iterator(xcb_get_setup(system->connection));
    system->screen = screens.data;
    if (system->screen == nullptr)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    system->wm_protocols = intern_atom(system->connection, "WM_PROTOCOLS");
    system->wm_delete_window = intern_atom(system->connection, "WM_DELETE_WINDOW");
    system->wm_size_hints = intern_atom(system->connection, "WM_SIZE_HINTS");
    system->net_wm_name = intern_atom(system->connection, "_NET_WM_NAME");
    system->utf8_string = intern_atom(system->connection, "UTF8_STRING");
    system->owner_thread = std::this_thread::get_id();
    system->backend = GRANIT_WINDOW_BACKEND_XCB;
    const auto handle = allocate_handle();
    std::lock_guard lock{registry_mutex};
    systems.emplace(handle, std::move(system));
    guard.release();
    *output = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_xcb_system(granit_window_system handle,
                                 const std::shared_ptr<window_system_record>& system) {
  for (const auto& [unused, window] : system->windows) {
    static_cast<void>(unused);
    if (window->window != XCB_WINDOW_NONE)
      xcb_destroy_window(system->connection, window->window);
  }
  system->windows.clear();
  {
    std::lock_guard lock{registry_mutex};
    systems.erase(handle);
  }
  xcb_flush(system->connection);
  xcb_disconnect(system->connection);
  return GRANIT_SUCCESS;
}

granit_result poll_xcb_event(const std::shared_ptr<window_system_record>& system,
                             granit_window_event* event) {
  pump_xcb_events(system);
  if (xcb_connection_has_error(system->connection) != 0)
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  if (system->events.empty())
    return GRANIT_ERROR_NOT_READY;
  *event = system->events.front();
  system->events.pop_front();
  return GRANIT_SUCCESS;
}

granit_result create_xcb_window(const std::shared_ptr<window_system_record>& system,
                                const granit_window_desc* desc, granit_window* output) {
  try {
    if (desc->width > UINT16_MAX || desc->height > UINT16_MAX ||
        (desc->title_length != 0 && desc->title == nullptr))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    auto record = std::make_shared<window_record>();
    record->handle = allocate_handle();
    record->system = system;
    record->width = desc->width;
    record->height = desc->height;
    record->framebuffer_width = desc->width;
    record->framebuffer_height = desc->height;
    record->window = xcb_generate_id(system->connection);
    const std::uint32_t values[] = {
        system->screen->black_pixel,
        XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_KEY_PRESS |
            XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
            XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW};
    xcb_create_window(system->connection, XCB_COPY_FROM_PARENT, record->window,
                      system->screen->root, 0, 0, static_cast<std::uint16_t>(desc->width),
                      static_cast<std::uint16_t>(desc->height), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      system->screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
    const char default_title[] = "Granit";
    const char* title = desc->title_length == 0 ? default_title : desc->title;
    const auto title_length = desc->title_length == 0 ? UINT32_C(6) : desc->title_length;
    xcb_change_property(system->connection, XCB_PROP_MODE_REPLACE, record->window, XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING, 8, title_length, title);
    if (system->net_wm_name != XCB_ATOM_NONE && system->utf8_string != XCB_ATOM_NONE)
      xcb_change_property(system->connection, XCB_PROP_MODE_REPLACE, record->window,
                          system->net_wm_name, system->utf8_string, 8, title_length, title);
    if (system->wm_protocols != XCB_ATOM_NONE && system->wm_delete_window != XCB_ATOM_NONE)
      xcb_change_property(system->connection, XCB_PROP_MODE_REPLACE, record->window,
                          system->wm_protocols, XCB_ATOM_ATOM, 32, 1, &system->wm_delete_window);
    if ((desc->flags & GRANIT_WINDOW_RESIZABLE_BIT) == 0 &&
        system->wm_size_hints != XCB_ATOM_NONE) {
      std::uint32_t hints[18]{};
      hints[0] = (UINT32_C(1) << 4) | (UINT32_C(1) << 5);
      hints[5] = hints[7] = desc->width;
      hints[6] = hints[8] = desc->height;
      xcb_change_property(system->connection, XCB_PROP_MODE_REPLACE, record->window,
                          XCB_ATOM_WM_NORMAL_HINTS, system->wm_size_hints, 32, 18, hints);
    }
    if ((desc->flags & GRANIT_WINDOW_VISIBLE_BIT) != 0)
      xcb_map_window(system->connection, record->window);
    system->windows.emplace(record->handle, record);
    if (xcb_flush(system->connection) <= 0 || xcb_connection_has_error(system->connection) != 0) {
      system->windows.erase(record->handle);
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    }
    *output = record->handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_xcb_window(const std::shared_ptr<window_system_record>& system,
                                 const std::shared_ptr<window_record>& window) {
  xcb_destroy_window(system->connection, window->window);
  return xcb_flush(system->connection) > 0 ? GRANIT_SUCCESS : GRANIT_ERROR_BACKEND_UNAVAILABLE;
}

granit_result get_xcb_window(const std::shared_ptr<window_system_record>& system,
                             const std::shared_ptr<window_record>& window, void** connection,
                             std::uint32_t* native_window) {
  *connection = system->connection;
  *native_window = window->window;
  return GRANIT_SUCCESS;
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
      const auto handle = public_handle(configured->window);
      const auto found = system->windows.find(handle);
      if (found != system->windows.end()) {
        found->second->width = configured->width;
        found->second->height = configured->height;
        found->second->framebuffer_width = configured->width;
        found->second->framebuffer_height = configured->height;
      }
      granit_window_event output = GRANIT_WINDOW_EVENT_INIT;
      output.type = GRANIT_WINDOW_EVENT_RESIZED;
      output.window = handle;
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
