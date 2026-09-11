// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_CONTENT_ID_HPP_
#define GRANIT_CORE_CONTENT_ID_HPP_

#include <array>
#include <cstddef>

#include <granit/core/content_id.h>

namespace granit {

/** 固定长度的 SHA-256 内容摘要。 */
using content_digest = std::array<std::byte, GRANIT_CONTENT_DIGEST_SIZE>;
/** 由规范化内容确定的资产身份。 */
using asset_content_id = content_digest;

} // namespace granit

#endif
