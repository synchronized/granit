// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/frame.h>

static granit_frame_info granit_test_frame_info = GRANIT_FRAME_INFO_INIT;
typedef char granit_frame_info_size_check
    [sizeof(granit_frame_info) == GRANIT_FRAME_INFO_VERSION_1_SIZE ? 1 : -1];

void granit_frame_h_header_test(void) { granit_test_frame_info.frame_slot = 0; }
