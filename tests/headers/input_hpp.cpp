// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/input.hpp>

#include <type_traits>

static_assert(std::is_standard_layout_v<granit::input_event>);
static_assert(sizeof(granit::input_event) == sizeof(granit_input_event));
static_assert(sizeof(granit::keyboard_state) == sizeof(granit_keyboard_state));
static_assert(sizeof(granit::pointer_state) == sizeof(granit_pointer_state));
static_assert(sizeof(granit::input_event) == 88);
static_assert(static_cast<std::uint32_t>(granit::input_event_type::text) ==
              GRANIT_INPUT_EVENT_TEXT);
static_assert(static_cast<std::uint32_t>(granit::key_action::repeated) ==
              GRANIT_KEY_ACTION_REPEATED);
