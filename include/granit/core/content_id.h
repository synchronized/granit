// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_CONTENT_ID_H_
#define GRANIT_CORE_CONTENT_ID_H_

#include <stdint.h>

#define GRANIT_CONTENT_DIGEST_SIZE UINT32_C(32)

/** 固定长度的 SHA-256 内容摘要。 */
typedef uint8_t granit_content_digest[GRANIT_CONTENT_DIGEST_SIZE];
/** 由规范化内容确定的资产身份。 */
typedef granit_content_digest granit_asset_content_id;

#endif
