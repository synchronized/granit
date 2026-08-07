// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/surface.h>

static granit_surface granit_test_surface;
static granit_win32_surface_desc granit_test_surface_desc = GRANIT_WIN32_SURFACE_DESC_INIT;

void granit_surface_h_header_test(void) {
  granit_test_surface = GRANIT_NULL_HANDLE;
  granit_test_surface_desc.window = 0;
}
