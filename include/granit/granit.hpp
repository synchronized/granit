// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_GRANIT_HPP_
#define GRANIT_GRANIT_HPP_

#include <cstdint>

#include <granit/granit.h>
#include <granit/renderer.hpp>
#include <granit/result.hpp>
#include <granit/surface.hpp>
#include <granit/swapchain.hpp>
#include <granit/types.hpp>

namespace granit {

struct version {
  std::uint32_t major;
  std::uint32_t minor;
  std::uint32_t patch;
};

[[nodiscard]] inline version library_version() noexcept {
  return {granit_version_major(), granit_version_minor(), granit_version_patch()};
}

} // namespace granit

#endif
