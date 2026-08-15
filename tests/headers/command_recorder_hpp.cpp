// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/command_recorder.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::command_recorder>);
static_assert(std::is_move_constructible_v<granit::command_recorder>);
static_assert(sizeof(granit::buffer_copy_region) == 24);
static_assert(sizeof(granit::texture_copy_region) == 64);
static_assert(requires(std::span<granit::command_recorder> recorders) {
  granit::command_recorder::submit_batch(recorders);
});
