// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/resource_types.hpp>

#include <cstdint>

constexpr auto buffer_flags =
    granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination;
static_assert(static_cast<std::uint32_t>(buffer_flags) ==
              (GRANIT_BUFFER_USAGE_VERTEX_BIT | GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT));
static_assert(static_cast<std::uint32_t>(granit::memory_location::upload) ==
              GRANIT_MEMORY_LOCATION_UPLOAD);
