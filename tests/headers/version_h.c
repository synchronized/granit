// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/core/version.h>

uint32_t granit_version_header_check(void) {
  return granit_version_major() + GRANIT_VERSION_MINOR + GRANIT_VERSION_PATCH;
}
