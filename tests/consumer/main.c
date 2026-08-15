// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>

#include "linkage_check.h"

int main(void) { return granit_version_major() == GRANIT_VERSION_MAJOR ? 0 : 1; }
