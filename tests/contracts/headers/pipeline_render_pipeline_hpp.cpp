// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/render_pipeline.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::render_pipeline>);
static_assert(std::is_move_constructible_v<granit::render_pipeline>);
