// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::pipeline_layout>);
static_assert(std::is_move_constructible_v<granit::pipeline_layout>);
static_assert(!std::is_copy_constructible_v<granit::bind_group_layout>);
static_assert(std::is_move_constructible_v<granit::bind_group_layout>);
static_assert(!std::is_copy_constructible_v<granit::graphics_pipeline>);
static_assert(std::is_move_constructible_v<granit::graphics_pipeline>);
