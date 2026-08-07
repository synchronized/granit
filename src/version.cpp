// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>

extern "C" uint32_t granit_version_major(void) { return GRANIT_VERSION_MAJOR; }

extern "C" uint32_t granit_version_minor(void) { return GRANIT_VERSION_MINOR; }

extern "C" uint32_t granit_version_patch(void) { return GRANIT_VERSION_PATCH; }
