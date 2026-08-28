// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/shared_library.h"

#include <utility>

namespace granit::detail::platform {
shared_library::~shared_library() { close(); }

shared_library::shared_library(shared_library&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)) {}

shared_library& shared_library::operator=(shared_library&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, nullptr);
  }
  return *this;
}

} // namespace granit::detail::platform
