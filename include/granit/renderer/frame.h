// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_FRAME_H_
#define GRANIT_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/types.h>

/** 一次 acquire 到 present 的短生命周期帧令牌。 */
typedef granit_handle granit_frame;

/** 成功获取的 Frame 所使用的真实在途帧槽信息。 */
typedef struct granit_frame_info {
  uint32_t struct_size;
  uint32_t frame_slot;
  uint32_t frame_slot_count;
  uint32_t reserved[5];
} granit_frame_info;

#define GRANIT_FRAME_INFO_VERSION_1_SIZE                                                           \
  ((uint32_t)(offsetof(granit_frame_info, reserved) + sizeof(((granit_frame_info*)0)->reserved)))

#define GRANIT_FRAME_INFO_INIT                                                                     \
  {                                                                                                \
    (uint32_t)sizeof(granit_frame_info), UINT32_C(0), UINT32_C(0), {                               \
      UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                              \
    }                                                                                              \
  }

#endif
