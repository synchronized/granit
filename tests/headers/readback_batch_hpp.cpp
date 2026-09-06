// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/readback_batch.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<granit::readback_batch>);
static_assert(!std::is_copy_constructible_v<granit::readback_batch>);
