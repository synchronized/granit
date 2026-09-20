// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_INPUT_STATE_UTILS_H_
#define GRANIT_WINDOW_INPUT_STATE_UTILS_H_

#include <granit/window/input.h>

#include <cstdint>

namespace granit::input::detail {

inline bool input_key_pressed(const granit_keyboard_state& state, std::uint32_t key) noexcept {
  return key < 256 && (state.pressed_keys[key / 64] & (UINT64_C(1) << (key % 64))) != 0;
}

inline std::uint32_t pressed_key_modifiers(const granit_keyboard_state& state) noexcept {
  std::uint32_t result = 0;
  const auto add = [&](std::uint32_t key, std::uint32_t bit) {
    if (input_key_pressed(state, key))
      result |= bit;
  };
  add(GRANIT_PHYSICAL_KEY_LEFT_SHIFT, GRANIT_MODIFIER_LEFT_SHIFT_BIT);
  add(GRANIT_PHYSICAL_KEY_RIGHT_SHIFT, GRANIT_MODIFIER_RIGHT_SHIFT_BIT);
  add(GRANIT_PHYSICAL_KEY_LEFT_CONTROL, GRANIT_MODIFIER_LEFT_CONTROL_BIT);
  add(GRANIT_PHYSICAL_KEY_RIGHT_CONTROL, GRANIT_MODIFIER_RIGHT_CONTROL_BIT);
  add(GRANIT_PHYSICAL_KEY_LEFT_ALT, GRANIT_MODIFIER_LEFT_ALT_BIT);
  add(GRANIT_PHYSICAL_KEY_RIGHT_ALT, GRANIT_MODIFIER_RIGHT_ALT_BIT);
  add(GRANIT_PHYSICAL_KEY_LEFT_SUPER, GRANIT_MODIFIER_LEFT_SUPER_BIT);
  add(GRANIT_PHYSICAL_KEY_RIGHT_SUPER, GRANIT_MODIFIER_RIGHT_SUPER_BIT);
  return result;
}

} // namespace granit::input::detail

#endif
