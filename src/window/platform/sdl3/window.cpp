// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "window/platform/backend.h"

#include <granit/renderer/native_surface.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#if defined(GRANIT_WINDOW_SDL3_HAS_X11)
#include <X11/Xlib-xcb.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <utility>

namespace granit::window::detail {
namespace {

std::weak_ptr<window_system_record> active_sdl3_system;

void destroy_native_window(SDL_Window* window) noexcept {
  if (window == nullptr)
    return;
#if defined(__EMSCRIPTEN__)
  // SDL 3.4.10 注销拖放事件时会重复删除 /tmp/filedrop。兼容处理只在单次窗口销毁期间
  // 忽略该目录的重复删除，并在返回后立即恢复 Emscripten FS 的原始实现。
  EM_ASM({
    const filedrop = "/tmp/filedrop";
    FS.mkdirTree(filedrop);
    Module.__granit_original_fs_rmdir = FS.rmdir;
    FS.rmdir = function(path) {
      try {
        return Module.__granit_original_fs_rmdir(path);
      } catch (error) {
        if (path === filedrop)
          return;
        throw error;
      }
    };
  });
#endif
  SDL_DestroyWindow(window);
#if defined(__EMSCRIPTEN__)
  EM_ASM({
    FS.rmdir = Module.__granit_original_fs_rmdir;
    delete Module.__granit_original_fs_rmdir;
  });
#endif
}

struct properties_owner {
  SDL_PropertiesID handle{};
  ~properties_owner() {
    if (handle != 0)
      SDL_DestroyProperties(handle);
  }
};

struct window_owner {
  SDL_Window* handle{};
  ~window_owner() {
    if (handle != nullptr)
      destroy_native_window(handle);
  }
  [[nodiscard]] SDL_Window* release() noexcept { return std::exchange(handle, nullptr); }
};

SDL_Window* native_window(const window_record& window) noexcept {
  return static_cast<SDL_Window*>(window.sdl_window);
}

std::int32_t fixed(float value) noexcept {
  const auto scaled = std::clamp(static_cast<double>(value) * 256.0,
                                 static_cast<double>(std::numeric_limits<std::int32_t>::min()),
                                 static_cast<double>(std::numeric_limits<std::int32_t>::max()));
  return static_cast<std::int32_t>(std::lround(scaled));
}

std::uint32_t pointer_buttons(SDL_MouseButtonFlags buttons) noexcept {
  std::uint32_t result = 0;
  if ((buttons & SDL_BUTTON_LMASK) != 0)
    result |= GRANIT_POINTER_PRIMARY_BIT;
  if ((buttons & SDL_BUTTON_RMASK) != 0)
    result |= GRANIT_POINTER_SECONDARY_BIT;
  if ((buttons & SDL_BUTTON_MMASK) != 0)
    result |= GRANIT_POINTER_MIDDLE_BIT;
  if ((buttons & SDL_BUTTON_X1MASK) != 0)
    result |= GRANIT_POINTER_X1_BIT;
  if ((buttons & SDL_BUTTON_X2MASK) != 0)
    result |= GRANIT_POINTER_X2_BIT;
  return result;
}

std::uint32_t pointer_button(std::uint8_t button) noexcept {
  switch (button) {
  case SDL_BUTTON_LEFT:
    return GRANIT_POINTER_PRIMARY_BIT;
  case SDL_BUTTON_RIGHT:
    return GRANIT_POINTER_SECONDARY_BIT;
  case SDL_BUTTON_MIDDLE:
    return GRANIT_POINTER_MIDDLE_BIT;
  case SDL_BUTTON_X1:
    return GRANIT_POINTER_X1_BIT;
  case SDL_BUTTON_X2:
    return GRANIT_POINTER_X2_BIT;
  default:
    return 0;
  }
}

std::uint32_t modifiers(SDL_Keymod modifiers) noexcept {
  std::uint32_t result = 0;
  const auto add = [&](SDL_Keymod source, std::uint32_t target) {
    if ((modifiers & source) != 0)
      result |= target;
  };
  add(SDL_KMOD_LSHIFT, GRANIT_MODIFIER_LEFT_SHIFT_BIT);
  add(SDL_KMOD_RSHIFT, GRANIT_MODIFIER_RIGHT_SHIFT_BIT);
  add(SDL_KMOD_LCTRL, GRANIT_MODIFIER_LEFT_CONTROL_BIT);
  add(SDL_KMOD_RCTRL, GRANIT_MODIFIER_RIGHT_CONTROL_BIT);
  add(SDL_KMOD_LALT, GRANIT_MODIFIER_LEFT_ALT_BIT);
  add(SDL_KMOD_RALT, GRANIT_MODIFIER_RIGHT_ALT_BIT);
  add(SDL_KMOD_LGUI, GRANIT_MODIFIER_LEFT_SUPER_BIT);
  add(SDL_KMOD_RGUI, GRANIT_MODIFIER_RIGHT_SUPER_BIT);
  add(SDL_KMOD_CAPS, GRANIT_MODIFIER_CAPS_LOCK_BIT);
  add(SDL_KMOD_NUM, GRANIT_MODIFIER_NUM_LOCK_BIT);
  return result;
}

std::uint32_t logical_key(SDL_Scancode key) noexcept {
  switch (key) {
  case SDL_SCANCODE_RETURN:
    return GRANIT_LOGICAL_KEY_ENTER;
  case SDL_SCANCODE_ESCAPE:
    return GRANIT_LOGICAL_KEY_ESCAPE;
  case SDL_SCANCODE_BACKSPACE:
    return GRANIT_LOGICAL_KEY_BACKSPACE;
  case SDL_SCANCODE_TAB:
    return GRANIT_LOGICAL_KEY_TAB;
  case SDL_SCANCODE_SPACE:
    return GRANIT_LOGICAL_KEY_SPACE;
  case SDL_SCANCODE_LEFT:
    return GRANIT_LOGICAL_KEY_LEFT;
  case SDL_SCANCODE_RIGHT:
    return GRANIT_LOGICAL_KEY_RIGHT;
  case SDL_SCANCODE_UP:
    return GRANIT_LOGICAL_KEY_UP;
  case SDL_SCANCODE_DOWN:
    return GRANIT_LOGICAL_KEY_DOWN;
  case SDL_SCANCODE_HOME:
    return GRANIT_LOGICAL_KEY_HOME;
  case SDL_SCANCODE_END:
    return GRANIT_LOGICAL_KEY_END;
  case SDL_SCANCODE_PAGEUP:
    return GRANIT_LOGICAL_KEY_PAGE_UP;
  case SDL_SCANCODE_PAGEDOWN:
    return GRANIT_LOGICAL_KEY_PAGE_DOWN;
  case SDL_SCANCODE_INSERT:
    return GRANIT_LOGICAL_KEY_INSERT;
  case SDL_SCANCODE_DELETE:
    return GRANIT_LOGICAL_KEY_DELETE;
  default:
    if (key >= SDL_SCANCODE_F1 && key <= SDL_SCANCODE_F12)
      return GRANIT_LOGICAL_KEY_F1 + static_cast<std::uint32_t>(key - SDL_SCANCODE_F1);
    return GRANIT_LOGICAL_KEY_NONE;
  }
}

std::shared_ptr<window_record> find_window(const std::shared_ptr<window_system_record>& system,
                                           SDL_WindowID id) {
  const auto mapped = system->sdl_windows.find(id);
  if (mapped == system->sdl_windows.end())
    return nullptr;
  const auto found = system->windows.find(mapped->second);
  return found == system->windows.end() ? nullptr : found->second;
}

granit_result refresh_window(window_record& window) noexcept {
  int width = 0;
  int height = 0;
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  auto* native = native_window(window);
  if (!SDL_GetWindowSize(native, &width, &height) ||
      !SDL_GetWindowSizeInPixels(native, &framebuffer_width, &framebuffer_height) || width <= 0 ||
      height <= 0 || framebuffer_width <= 0 || framebuffer_height <= 0) {
    return GRANIT_ERROR_SURFACE_LOST;
  }
  window.width = static_cast<std::uint32_t>(width);
  window.height = static_cast<std::uint32_t>(height);
  window.framebuffer_width = static_cast<std::uint32_t>(framebuffer_width);
  window.framebuffer_height = static_cast<std::uint32_t>(framebuffer_height);
  window.content_scale_horizontal =
      static_cast<float>(framebuffer_width) / static_cast<float>(width);
  window.content_scale_vertical =
      static_cast<float>(framebuffer_height) / static_cast<float>(height);
  return GRANIT_SUCCESS;
}

void enqueue_resize(const std::shared_ptr<window_system_record>& system,
                    const window_record& window) {
  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  event.type = GRANIT_WINDOW_EVENT_RESIZED;
  event.window = window.handle;
  event.timestamp_ns = timestamp_ns();
  event.data.resized.width = window.width;
  event.data.resized.height = window.height;
  system->events.push_back(event);
}

void enqueue_focus(const std::shared_ptr<window_system_record>& system, const window_record& window,
                   bool focused) {
  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  event.type = GRANIT_WINDOW_EVENT_FOCUS_CHANGED;
  event.window = window.handle;
  event.timestamp_ns = timestamp_ns();
  event.data.focus.focused = focused ? UINT32_C(1) : UINT32_C(0);
  system->events.push_back(event);
}

void handle_input(window_system_record& system, granit_window window, const SDL_Event& source) {
  granit_window_input_native_event event{};
  event.backend = GRANIT_WINDOW_INPUT_BACKEND_SDL3;
  switch (source.type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
    event.type = source.type == SDL_EVENT_KEY_DOWN ? GRANIT_WINDOW_INPUT_SDL3_KEY_DOWN
                                                   : GRANIT_WINDOW_INPUT_SDL3_KEY_UP;
    event.detail = static_cast<std::uint32_t>(source.key.scancode);
    event.state = modifiers(source.key.mod);
    event.data0 = source.key.repeat ? UINT32_C(1) : UINT32_C(0);
    event.data1 = logical_key(source.key.scancode);
    break;
  case SDL_EVENT_TEXT_INPUT:
    event.type = GRANIT_WINDOW_INPUT_SDL3_TEXT;
    event.word = reinterpret_cast<std::uintptr_t>(source.text.text);
    event.value =
        source.text.text == nullptr ? 0 : static_cast<std::intptr_t>(std::strlen(source.text.text));
    break;
  case SDL_EVENT_MOUSE_MOTION:
    event.type = GRANIT_WINDOW_INPUT_SDL3_POINTER_MOTION;
    event.x = fixed(source.motion.x);
    event.y = fixed(source.motion.y);
    event.value = fixed(source.motion.xrel);
    event.data0 = static_cast<std::uint32_t>(fixed(source.motion.yrel));
    event.state = pointer_buttons(source.motion.state);
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
    event.type = source.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? GRANIT_WINDOW_INPUT_SDL3_POINTER_DOWN
                                                            : GRANIT_WINDOW_INPUT_SDL3_POINTER_UP;
    event.x = fixed(source.button.x);
    event.y = fixed(source.button.y);
    event.detail = pointer_button(source.button.button);
    if (event.detail == 0)
      return;
    if (const auto found = system.pointers.find(window); found != system.pointers.end())
      event.state = found->second.buttons;
    if (source.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
      event.state |= event.detail;
    else
      event.state &= ~event.detail;
    break;
  case SDL_EVENT_MOUSE_WHEEL: {
    event.type = GRANIT_WINDOW_INPUT_SDL3_POINTER_WHEEL;
    event.x = fixed(source.wheel.mouse_x);
    event.y = fixed(source.wheel.mouse_y);
    const auto direction = source.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
    event.value = fixed(source.wheel.x * direction);
    event.data0 = static_cast<std::uint32_t>(fixed(source.wheel.y * direction));
    break;
  }
  case SDL_EVENT_WINDOW_MOUSE_ENTER:
    event.type = GRANIT_WINDOW_INPUT_SDL3_POINTER_ENTER;
    break;
  case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    event.type = GRANIT_WINDOW_INPUT_SDL3_POINTER_LEAVE;
    break;
  default:
    return;
  }
  handle_native_input(system, window, event);
}

SDL_WindowID event_window(const SDL_Event& event) noexcept {
  switch (event.type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
    return event.key.windowID;
  case SDL_EVENT_TEXT_INPUT:
    return event.text.windowID;
  case SDL_EVENT_MOUSE_MOTION:
    return event.motion.windowID;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
    return event.button.windowID;
  case SDL_EVENT_MOUSE_WHEEL:
    return event.wheel.windowID;
  default:
    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST)
      return event.window.windowID;
    return 0;
  }
}

} // namespace

granit_result create_sdl3_system(granit_window_system* output) {
  if (!active_sdl3_system.expired())
    return GRANIT_ERROR_RESOURCE_IN_USE;
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  try {
    auto system = std::make_shared<window_system_record>();
    system->owner_thread = std::this_thread::get_id();
    system->backend = GRANIT_WINDOW_BACKEND_SDL3;
    const auto handle = allocate_handle();
    {
      std::lock_guard lock{registry_mutex};
      systems.emplace(handle, system);
    }
    active_sdl3_system = system;
    *output = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_sdl3_system(granit_window_system handle,
                                  const std::shared_ptr<window_system_record>& system) {
  for (const auto& [unused, window] : system->windows) {
    static_cast<void>(unused);
    if (window != nullptr && window->sdl_window != nullptr)
      destroy_native_window(native_window(*window));
  }
  system->sdl_windows.clear();
  system->windows.clear();
  {
    std::lock_guard lock{registry_mutex};
    systems.erase(handle);
  }
  active_sdl3_system.reset();
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return GRANIT_SUCCESS;
}

granit_result process_sdl3_events(const std::shared_ptr<window_system_record>& system) {
  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      for (const auto& [unused, window] : system->windows) {
        static_cast<void>(unused);
        if (window != nullptr)
          enqueue_event(system, window->handle, GRANIT_WINDOW_EVENT_CLOSE_REQUESTED);
      }
      continue;
    }
    const auto window = find_window(system, event_window(event));
    if (window == nullptr)
      continue;
    switch (event.type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      enqueue_event(system, window->handle, GRANIT_WINDOW_EVENT_CLOSE_REQUESTED);
      break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
      const auto old_width = window->width;
      const auto old_height = window->height;
      const auto old_framebuffer_width = window->framebuffer_width;
      const auto old_framebuffer_height = window->framebuffer_height;
      if (refresh_window(*window) == GRANIT_SUCCESS &&
          (window->width != old_width || window->height != old_height ||
           window->framebuffer_width != old_framebuffer_width ||
           window->framebuffer_height != old_framebuffer_height)) {
        enqueue_resize(system, *window);
      }
      break;
    }
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      if (refresh_window(*window) == GRANIT_SUCCESS) {
        granit_window_event scale = GRANIT_WINDOW_EVENT_INIT;
        scale.type = GRANIT_WINDOW_EVENT_SCALE_CHANGED;
        scale.window = window->handle;
        scale.timestamp_ns = timestamp_ns();
        scale.data.scale = {window->content_scale_horizontal, window->content_scale_vertical,
                            window->framebuffer_width, window->framebuffer_height};
        system->events.push_back(scale);
      }
      break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      enqueue_focus(system, *window, true);
      break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      clear_input_focus(*system, window->handle);
      enqueue_focus(system, *window, false);
      break;
    default:
      handle_input(*system, window->handle, event);
      break;
    }
  }
  return GRANIT_SUCCESS;
}

granit_result create_sdl3_window(const std::shared_ptr<window_system_record>& system,
                                 const granit_window_desc* desc, granit_window* output) {
#if !defined(__EMSCRIPTEN__)
  if (desc->struct_size >= GRANIT_WINDOW_DESC_VERSION_2_SIZE && desc->target != nullptr &&
      desc->target->type != GRANIT_WINDOW_TARGET_AUTOMATIC) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
#endif
  try {
    const std::string title = desc->title_length == 0
                                  ? std::string{"Granit"}
                                  : std::string{desc->title, desc->title_length};
    properties_owner properties{SDL_CreateProperties()};
    if (properties.handle == 0)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    if (!SDL_SetStringProperty(properties.handle, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                               title.c_str()) ||
        !SDL_SetNumberProperty(properties.handle, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
                               desc->width) ||
        !SDL_SetNumberProperty(properties.handle, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                               desc->height) ||
        !SDL_SetBooleanProperty(properties.handle, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN,
                                (desc->flags & GRANIT_WINDOW_VISIBLE_BIT) == 0) ||
        !SDL_SetBooleanProperty(properties.handle, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN,
                                (desc->flags & GRANIT_WINDOW_RESIZABLE_BIT) != 0) ||
        !SDL_SetBooleanProperty(properties.handle,
                                SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN,
                                (desc->flags & GRANIT_WINDOW_HIGH_DPI_BIT) != 0)) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
#if defined(__EMSCRIPTEN__)
    std::string canvas_selector = "#canvas";
    if (desc->struct_size >= GRANIT_WINDOW_DESC_VERSION_2_SIZE && desc->target != nullptr &&
        desc->target->type == GRANIT_WINDOW_TARGET_CANVAS_SELECTOR) {
      canvas_selector.assign(desc->target->value, desc->target->value_length);
    }
    for (const auto& [unused, window] : system->windows) {
      static_cast<void>(unused);
      if (window != nullptr && window->sdl_canvas_selector == canvas_selector) {
        return GRANIT_ERROR_RESOURCE_IN_USE;
      }
    }
    if (!SDL_SetStringProperty(properties.handle,
                               SDL_PROP_WINDOW_CREATE_EMSCRIPTEN_CANVAS_ID_STRING,
                               canvas_selector.c_str())) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
#endif
    window_owner native{SDL_CreateWindowWithProperties(properties.handle)};
    if (native.handle == nullptr)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    auto window = std::make_shared<window_record>();
    window->handle = allocate_handle();
    window->system = system;
    window->sdl_window = native.handle;
    window->sdl_window_id = SDL_GetWindowID(native.handle);
    if (window->sdl_window_id == 0)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
#if defined(__EMSCRIPTEN__)
    const auto native_properties = SDL_GetWindowProperties(native.handle);
    const auto* actual_selector = SDL_GetStringProperty(
        native_properties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, canvas_selector.c_str());
    window->sdl_canvas_selector = actual_selector;
#endif
    const auto state_result = refresh_window(*window);
    if (state_result != GRANIT_SUCCESS)
      return state_result;
    if (!SDL_StartTextInput(native.handle))
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    system->windows.emplace(window->handle, window);
    try {
      system->sdl_windows.emplace(window->sdl_window_id, window->handle);
    } catch (...) {
      system->windows.erase(window->handle);
      throw;
    }
    static_cast<void>(native.release());
    *output = window->handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_sdl3_window(const std::shared_ptr<window_system_record>& system,
                                  granit_window handle) {
  const auto found = system->windows.find(handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto window = std::move(found->second);
  system->windows.erase(found);
  system->sdl_windows.erase(window->sdl_window_id);
  SDL_StopTextInput(native_window(*window));
  destroy_native_window(native_window(*window));
  return GRANIT_SUCCESS;
}

granit_result create_sdl3_surface(const std::shared_ptr<window_system_record>&,
                                  const std::shared_ptr<window_record>& window,
                                  granit_renderer renderer, granit_surface* surface) {
  const auto properties = SDL_GetWindowProperties(native_window(*window));
  const auto* driver = SDL_GetCurrentVideoDriver();
  granit_surface_desc desc = GRANIT_SURFACE_DESC_INIT;
  if (driver != nullptr && std::strcmp(driver, "windows") == 0) {
    desc.surface_type = GRANIT_SURFACE_TYPE_WIN32_BIT;
    desc.source.win32.instance =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
    desc.source.win32.window =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
  } else if (driver != nullptr && std::strcmp(driver, "wayland") == 0) {
    desc.surface_type = GRANIT_SURFACE_TYPE_WAYLAND_BIT;
    desc.source.wayland.display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    desc.source.wayland.surface =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
#if defined(GRANIT_WINDOW_SDL3_HAS_X11)
  } else if (driver != nullptr && std::strcmp(driver, "x11") == 0) {
    auto* display = static_cast<Display*>(
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
    const auto native = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (display == nullptr || native <= 0 || native > std::numeric_limits<std::uint32_t>::max())
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    desc.surface_type = GRANIT_SURFACE_TYPE_XCB_BIT;
    desc.source.xcb.connection = XGetXCBConnection(display);
    desc.source.xcb.window = static_cast<std::uint32_t>(native);
#endif
  } else if (driver != nullptr && std::strcmp(driver, "emscripten") == 0) {
    desc.surface_type = GRANIT_SURFACE_TYPE_CANVAS_BIT;
    desc.source.canvas.selector = window->sdl_canvas_selector.data();
    desc.source.canvas.selector_length =
        static_cast<std::uint32_t>(window->sdl_canvas_selector.size());
  } else {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return granit_surface_create(renderer, &desc, surface);
}

} // namespace granit::window::detail
