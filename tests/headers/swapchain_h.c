// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/swapchain.h>

static granit_swapchain_desc granit_test_swapchain_desc = GRANIT_SWAPCHAIN_DESC_INIT;
static granit_swapchain_info granit_test_swapchain_info = GRANIT_SWAPCHAIN_INFO_INIT;

void granit_swapchain_h_header_test(void) {
  granit_test_swapchain_desc.width = 1;
  granit_test_swapchain_info.image_count = 0;
}
