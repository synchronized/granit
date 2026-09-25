// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.hpp>

#include <utility>

namespace {

[[nodiscard]] bool renderer_unavailable(granit::result value) noexcept {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device || value == granit::result::unsupported;
}

} // namespace

int main() {
  granit::window_system system;
  const auto result = system.initialize();
  if (result == granit::result::unsupported || result == granit::result::backend_unavailable) {
    return system.valid() ? 1 : 0;
  }
  if (result.failed() || !system.valid())
    return 2;

  granit::window window;
  if ((window.initialize(system, {.title = "Granit Consumer A",
                                  .width = 320,
                                  .height = 240,
                                  .flags = granit::window_flag::none}))
          .failed() ||
      !window.valid())
    return 3;

  granit::window second_window;
  if ((second_window.initialize(system, {.title = "Granit Consumer B",
                                         .width = 160,
                                         .height = 120,
                                         .flags = granit::window_flag::none}))
          .failed() ||
      !second_window.valid() || second_window.ref() == window.ref())
    return 4;

  granit::window_state state;
  if ((window.get_state(state)).failed() || state.width == 0 || state.height == 0)
    return 5;
  granit::window_state second_state;
  if ((second_window.get_state(second_state)).failed() || second_state.width == 0 ||
      second_state.height == 0)
    return 6;

  granit::renderer renderer;
  const auto renderer_result =
      renderer.initialize({.application_name = "Granit Window Consumer",
                           .presentation = granit::presentation_mode::enabled});
  if (renderer_unavailable(renderer_result)) {
    if (renderer.valid())
      return 7;
  } else {
    if (renderer_result.failed() || !renderer.valid())
      return 8;
    granit::surface surface;
    if ((window.create_surface(renderer, surface)).failed() || !surface.valid())
      return 9;
    if ((surface.reset()).failed() || surface.valid())
      return 10;
    if ((window.create_surface(renderer, surface)).failed() || !surface.valid())
      return 11;
    if ((surface.reset()).failed() || (surface.reset()).failed())
      return 12;
    if ((renderer.reset()).failed() || (renderer.reset()).failed())
      return 13;
  }

  granit::window moved = std::move(window);
  if (window.valid() || !moved.valid())
    return 14;
  if ((moved.reset()).failed() || (moved.reset()).failed())
    return 15;
  const granit::window_state sentinel{.width = 11, .height = 22};
  state = sentinel;
  if (moved.get_state(state) != granit::result::invalid_handle || state.width != sentinel.width ||
      state.height != sentinel.height)
    return 16;
  if ((second_window.reset()).failed() || (second_window.reset()).failed())
    return 17;

  granit::window_system moved_system = std::move(system);
  if (system.valid() || !moved_system.valid())
    return 18;
  if ((moved_system.reset()).failed() || (moved_system.reset()).failed())
    return 19;
  return 0;
}
