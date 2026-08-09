// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/command_recorder.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::command_recorder>);
static_assert(std::is_move_constructible_v<granit::command_recorder>);
