// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/swapchain.h>

static granit_swapchain_desc granit_test_swapchain_desc = GRANIT_SWAPCHAIN_DESC_INIT;
static granit_swapchain_info granit_test_swapchain_info = GRANIT_SWAPCHAIN_INFO_INIT;
static granit_frame_info granit_test_frame_info = GRANIT_FRAME_INFO_INIT;
typedef char granit_swapchain_info_size_check
    [sizeof(granit_swapchain_info) >= GRANIT_SWAPCHAIN_INFO_SIZE ? 1 : -1];
typedef char granit_frame_info_size_check
    [sizeof(granit_frame_info) == GRANIT_FRAME_INFO_VERSION_1_SIZE ? 1 : -1];

void granit_swapchain_h_header_test(void) {
  granit_test_swapchain_desc.width = 1;
  granit_test_swapchain_info.image_count = 0;
  granit_test_swapchain_info.format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  granit_test_frame_info.frame_slot = 0;
}
