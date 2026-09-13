// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_SHA256_H_
#define GRANIT_CORE_SHA256_H_

#include <granit/core/content_id.hpp>

#include <cstddef>
#include <span>

namespace granit::detail {

content_digest sha256_bytes(std::span<const std::byte> bytes) noexcept;
content_digest sha256_bytes(std::span<const std::span<const std::byte>> segments) noexcept;
content_digest sha256_bytes_with_zeroed_range(std::span<const std::byte> bytes,
                                              std::size_t offset,
                                              std::size_t size) noexcept;

} // namespace granit::detail

#endif
