// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/shared_library.h"

namespace granit::detail::platform {

bool shared_library::open(const char*) noexcept {
  close();
  return false;
}

void shared_library::close() noexcept { handle_ = nullptr; }

void* shared_library::symbol(const char*) const noexcept { return nullptr; }

} // namespace granit::detail::platform
