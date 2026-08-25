// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include "linkage_check.h"

#include <string>
#include <utility>

int main() {
  const auto runtime = granit::library_version();
  const auto header = std::to_string(GRANIT_VERSION_MAJOR) + "." +
                      std::to_string(GRANIT_VERSION_MINOR) + "." +
                      std::to_string(GRANIT_VERSION_PATCH);
  if (header != GRANIT_CONSUMER_PACKAGE_VERSION)
    return 1;
  if (runtime.major != GRANIT_VERSION_MAJOR || runtime.minor != GRANIT_VERSION_MINOR ||
      runtime.patch != GRANIT_VERSION_PATCH)
    return 2;

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize();
  if (renderer_result == granit::result::backend_unavailable ||
      renderer_result == granit::result::incompatible_driver ||
      renderer_result == granit::result::no_suitable_device)
    return renderer.valid() ? 3 : 0;
  if (granit::failed(renderer_result))
    return 4;

  granit::buffer buffer;
  if (granit::failed(buffer.initialize(renderer.native_handle(),
                                       {.size = 64,
                                        .usage = granit::buffer_usage::transfer_source,
                                        .location = granit::memory_location::upload})))
    return 5;
  granit::buffer moved = std::move(buffer);
  if (buffer.valid() || !moved.valid())
    return 6;
  if (granit::failed(moved.reset()) || granit::failed(moved.reset()))
    return 7;
  granit::renderer moved_renderer = std::move(renderer);
  if (renderer.valid() || !moved_renderer.valid())
    return 8;
  if (granit::failed(moved_renderer.reset()) || granit::failed(moved_renderer.reset()))
    return 9;
  return 0;
}
