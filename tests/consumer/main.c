// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>

#include "linkage_check.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  char header_version[32] = {0};
  (void)snprintf(header_version, sizeof(header_version), "%u.%u.%u", GRANIT_VERSION_MAJOR,
                 GRANIT_VERSION_MINOR, GRANIT_VERSION_PATCH);
  if (strcmp(header_version, GRANIT_CONSUMER_PACKAGE_VERSION) != 0)
    return 1;
  return granit_version_major() == GRANIT_VERSION_MAJOR &&
                 granit_version_minor() == GRANIT_VERSION_MINOR &&
                 granit_version_patch() == GRANIT_VERSION_PATCH
             ? 0
             : 2;
}
