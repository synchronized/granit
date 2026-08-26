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
  if (granit::failed(window_result))
    return 1;

  granit::input_system input;
  if (granit::failed(input.initialize(windows.native_handle())))
    return 2;
  if (granit::failed(input.reset()) || granit::failed(input.reset()))
    return 3;
  if (granit::failed(windows.reset()) || granit::failed(windows.reset()))
    return 4;
  return 0;
}
