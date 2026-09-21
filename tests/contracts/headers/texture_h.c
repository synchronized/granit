// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include <granit/renderer/texture.h>
typedef char granit_texture_format_footprint_size_check
    [(sizeof(granit_texture_format_footprint) ==
      GRANIT_TEXTURE_FORMAT_FOOTPRINT_VERSION_1_SIZE)
         ? 1
         : -1];
typedef char granit_texture_readback_info_size_check
    [(sizeof(granit_texture_readback_info) == GRANIT_TEXTURE_READBACK_INFO_VERSION_1_SIZE) ? 1
                                                                                          : -1];
granit_texture granit_texture_header_check(void) { return GRANIT_NULL_HANDLE; }
