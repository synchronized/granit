// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/surface.h>

static granit_surface granit_test_surface;
static granit_win32_surface_desc granit_test_surface_desc = GRANIT_WIN32_SURFACE_DESC_INIT;
static granit_xcb_surface_desc granit_test_xcb_surface_desc = GRANIT_XCB_SURFACE_DESC_INIT;
static granit_wayland_surface_desc granit_test_wayland_surface_desc =
    GRANIT_WAYLAND_SURFACE_DESC_INIT;
static granit_canvas_surface_desc granit_test_canvas_surface_desc = GRANIT_CANVAS_SURFACE_DESC_INIT;

void granit_surface_h_header_test(void) {
  granit_test_surface = GRANIT_NULL_HANDLE;
  granit_test_surface_desc.window = 0;
  granit_test_xcb_surface_desc.window = 0;
  granit_test_wayland_surface_desc.surface = 0;
  granit_test_canvas_surface_desc.selector = 0;
}
