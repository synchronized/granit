// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/pipeline_warmup.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<granit::pipeline_warmup_batch>);
static_assert(!std::is_copy_constructible_v<granit::pipeline_warmup_batch>);
static_assert(std::is_trivially_copyable_v<granit::pipeline_warmup_batch_ref>);
