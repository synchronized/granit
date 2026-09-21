// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_PRESENTATION_HPP_
#define GRANIT_WINDOW_PRESENTATION_HPP_

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/window/presentation.h>
#include <granit/window/window.hpp>

namespace granit {

inline result window::create_surface(renderer& owner, surface& output) const noexcept {
  if (output.valid())
    return result::invalid_argument;
  const auto renderer = owner.native_handle();
  granit_surface handle = GRANIT_NULL_HANDLE;
  const auto value = granit_window_create_surface(system_, handle_, renderer, &handle);
  if (value == GRANIT_SUCCESS)
    output.adopt(renderer, handle);
  return from_native(value);
}

} // namespace granit

#endif
