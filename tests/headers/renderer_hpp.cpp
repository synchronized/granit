// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<granit::renderer>);
static_assert(!std::is_copy_constructible_v<granit::renderer>);
static_assert(sizeof(granit_renderer_desc) >= GRANIT_RENDERER_DESC_SIZE);
static_assert(sizeof(granit_renderer_limits) == GRANIT_RENDERER_LIMITS_VERSION_1_SIZE);
static_assert(std::is_aggregate_v<granit::renderer_limits>);
static_assert(granit::renderer_limits{.framebuffer_sample_counts = GRANIT_SAMPLE_COUNT_1 |
                                                                   GRANIT_SAMPLE_COUNT_4}
                  .supports_sample_count(granit::sample_count::four));
