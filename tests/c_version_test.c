// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>

int main(void) {
  return sizeof(granit_handle) == sizeof(uint64_t) && GRANIT_NULL_HANDLE == 0 &&
             granit_result_message(GRANIT_SUCCESS) != 0 &&
             granit_result_message(GRANIT_ERROR_INVALID_ARGUMENT) != 0 &&
             granit_version_major() == GRANIT_VERSION_MAJOR &&
             granit_version_minor() == GRANIT_VERSION_MINOR &&
             granit_version_patch() == GRANIT_VERSION_PATCH
           ? 0
           : 1;
}
