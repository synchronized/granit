// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include "linkage_check.h"

#include <string>

int main() {
  const auto runtime = granit::library_version();
  const auto header = std::to_string(GRANIT_VERSION_MAJOR) + "." +
                      std::to_string(GRANIT_VERSION_MINOR) + "." +
                      std::to_string(GRANIT_VERSION_PATCH);
  if (header != GRANIT_CONSUMER_PACKAGE_VERSION)
    return 1;
  return runtime.major == GRANIT_VERSION_MAJOR && runtime.minor == GRANIT_VERSION_MINOR &&
                 runtime.patch == GRANIT_VERSION_PATCH
             ? 0
             : 2;
}
