// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.hpp>

#include <utility>

int main() {
  granit::window_system system;
  const auto result = system.initialize();
  if (result == granit::result::unsupported || result == granit::result::backend_unavailable) {
    return system.valid() ? 1 : 0;
  }
  if (result.failed() || !system.valid())
    return 2;

  granit::window window;
  if ((window.initialize(system, {.width = 320, .height = 240})).failed() || !window.valid())
    return 3;

  granit::window_state state;
  if ((window.get_state(state)).failed() || state.width == 0 || state.height == 0)
    return 4;

  granit::window moved = std::move(window);
  if (window.valid() || !moved.valid())
    return 5;
  if ((moved.reset()).failed() || (moved.reset()).failed())
    return 6;

  granit::window_system moved_system = std::move(system);
  if (system.valid() || !moved_system.valid())
    return 7;
  if ((moved_system.reset()).failed() || (moved_system.reset()).failed())
    return 8;
  return 0;
}
