// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.hpp>

int main() {
  granit::window_system windows;
  const auto window_result = windows.initialize();
  if (window_result == granit::result::unsupported ||
      window_result == granit::result::backend_unavailable)
    return 0;
  if (window_result.failed())
    return 1;

  if ((windows.process_events()).failed())
    return 2;
  granit::input_event event = GRANIT_INPUT_EVENT_INIT;
  if (windows.poll(event) != granit::result::not_ready)
    return 3;
  if ((windows.reset()).failed() || (windows.reset()).failed())
    return 4;
  return 0;
}
