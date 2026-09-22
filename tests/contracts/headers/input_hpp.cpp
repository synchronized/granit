// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/input.hpp>

#include <type_traits>

static_assert(std::is_standard_layout_v<granit::input_event>);
static_assert(std::is_same_v<decltype(granit::input_event{}.type), granit::input_event_type>);
static_assert(
    std::is_same_v<decltype(granit::input_event{}.data.key.physical), granit::physical_key>);
static_assert(
    std::is_same_v<decltype(granit::input_event{}.data.key.logical), granit::logical_key>);
static_assert(std::is_same_v<decltype(granit::input_event{}.data.key.action), granit::key_action>);
static_assert(sizeof(granit::input_event) == sizeof(granit_input_event));
static_assert(std::is_same_v<decltype(granit::keyboard_state{}.pressed_keys),
                             std::array<std::uint64_t, 4>>);
static_assert(std::is_same_v<decltype(granit::pointer_state{}.inside), bool>);
static_assert(sizeof(granit::input_event) == 88);
static_assert(static_cast<std::uint32_t>(granit::input_event_type::text) ==
              GRANIT_INPUT_EVENT_TEXT);
static_assert(static_cast<std::uint32_t>(granit::key_action::repeated) ==
              GRANIT_KEY_ACTION_REPEATED);
static_assert(static_cast<std::uint32_t>(granit::physical_key::escape) ==
              GRANIT_PHYSICAL_KEY_ESCAPE);
