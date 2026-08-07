// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>

int main(void) {
  return granit_version_major() == GRANIT_VERSION_MAJOR &&
             granit_version_minor() == GRANIT_VERSION_MINOR &&
             granit_version_patch() == GRANIT_VERSION_PATCH
           ? 0
           : 1;
}
