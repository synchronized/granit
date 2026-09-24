// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.hpp>

#include <emscripten/emscripten.h>

#include <cstdio>
#include <string_view>

extern "C" EMSCRIPTEN_KEEPALIVE int granit_web_validate_sdl3_multi_window() noexcept {
  granit::window_system system;
  auto result = system.initialize({.backend = granit::window_backend::sdl3});
  if (result.failed())
    return granit::to_native(result);

  const auto create = [&](granit::window& window, std::string_view selector) {
    return window.initialize(
        system, {.title = "SDL3 Canvas",
                 .width = 320,
                 .height = 180,
                 .flags = granit::window_flag::visible | granit::window_flag::resizable,
                 .target = granit::window_target::canvas(selector)});
  };
  granit::window primary;
  granit::window secondary;
  result = create(primary, "#sdl-primary-canvas");
  if (result.ok())
    result = create(secondary, "#sdl-secondary-canvas");
  if (result.failed())
    return granit::to_native(result);

  granit::window_state primary_state;
  granit::window_state secondary_state;
  result = primary.get_state(primary_state);
  if (result.ok())
    result = secondary.get_state(secondary_state);
  if (result.failed() || primary_state.width != 320 || primary_state.height != 180 ||
      secondary_state.width != 400 || secondary_state.height != 200) {
    std::fprintf(stderr, "GRANIT_WINDOW_SDL3_TARGET:state:%d:%u:%u:%u:%u\n",
                 granit::to_native(result), primary_state.width, primary_state.height,
                 secondary_state.width, secondary_state.height);
    return GRANIT_ERROR_INTERNAL;
  }

  granit::window duplicate;
  const auto duplicate_result = create(duplicate, "#sdl-secondary-canvas");
  if (duplicate_result != granit::result::resource_in_use) {
    std::fprintf(stderr, "GRANIT_WINDOW_SDL3_TARGET:duplicate:%d\n",
                 granit::to_native(duplicate_result));
    return GRANIT_ERROR_INTERNAL;
  }
  result = secondary.reset();
  if (result.ok())
    result = create(secondary, "#sdl-secondary-canvas");
  return granit::to_native(result);
}
