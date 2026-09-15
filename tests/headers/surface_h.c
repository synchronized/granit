// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/native_surface.h>

static granit_surface granit_test_surface;
static granit_surface_desc granit_test_surface_desc = GRANIT_SURFACE_DESC_INIT;

granit_surface granit_surface_h_header_test(void) {
  granit_test_surface = GRANIT_NULL_HANDLE;
  granit_test_surface_desc.surface_type = GRANIT_SURFACE_TYPE_WIN32_BIT;
  granit_test_surface_desc.source.win32.window = 0;
  return granit_test_surface;
}
