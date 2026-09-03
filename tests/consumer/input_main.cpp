// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/input.hpp>
#include <granit/window.hpp>

int main() {
  granit::window_system windows;
  const auto window_result = windows.initialize();
  if (window_result == granit::result::unsupported ||
      window_result == granit::result::backend_unavailable)
    return 0;
  if (window_result.failed())
    return 1;

  granit::input_system input;
  if ((input.initialize(windows.native_handle())).failed())
    return 2;
  if ((input.reset()).failed() || (input.reset()).failed())
    return 3;
  if ((windows.reset()).failed() || (windows.reset()).failed())
    return 4;
  return 0;
}
