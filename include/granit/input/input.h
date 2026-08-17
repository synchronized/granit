// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INPUT_INPUT_H_
#define GRANIT_INPUT_INPUT_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/input/export.h>
#include <granit/window/window.h>

typedef granit_handle granit_input_system;

typedef struct granit_input_system_desc {
  uint32_t struct_size;
  granit_window_system window_system;
  uint32_t flags;
  uint32_t reserved;
} granit_input_system_desc;

#define GRANIT_INPUT_SYSTEM_DESC_VERSION_1_SIZE                                                    \
  ((uint32_t)(offsetof(granit_input_system_desc, reserved) + sizeof(uint32_t)))
#define GRANIT_INPUT_SYSTEM_DESC_INIT                                                              \
  {(uint32_t)sizeof(granit_input_system_desc), GRANIT_NULL_HANDLE, UINT32_C(0), UINT32_C(0)}

typedef enum granit_input_event_type {
  GRANIT_INPUT_EVENT_KEY = 1,
  GRANIT_INPUT_EVENT_TEXT = 2,
  GRANIT_INPUT_EVENT_POINTER_MOVED = 3,
  GRANIT_INPUT_EVENT_POINTER_BUTTON = 4,
  GRANIT_INPUT_EVENT_POINTER_WHEEL = 5,
  GRANIT_INPUT_EVENT_POINTER_ENTERED = 6,
  GRANIT_INPUT_EVENT_POINTER_LEFT = 7
} granit_input_event_type;

/** USB HID Keyboard/Keypad usage 对应的稳定物理键位置。 */
typedef enum granit_physical_key {
  GRANIT_PHYSICAL_KEY_UNKNOWN = 0,
  GRANIT_PHYSICAL_KEY_A = 4,
  GRANIT_PHYSICAL_KEY_B = 5,
  GRANIT_PHYSICAL_KEY_C = 6,
  GRANIT_PHYSICAL_KEY_D = 7,
  GRANIT_PHYSICAL_KEY_E = 8,
  GRANIT_PHYSICAL_KEY_F = 9,
  GRANIT_PHYSICAL_KEY_G = 10,
  GRANIT_PHYSICAL_KEY_H = 11,
  GRANIT_PHYSICAL_KEY_I = 12,
  GRANIT_PHYSICAL_KEY_J = 13,
  GRANIT_PHYSICAL_KEY_K = 14,
  GRANIT_PHYSICAL_KEY_L = 15,
  GRANIT_PHYSICAL_KEY_M = 16,
  GRANIT_PHYSICAL_KEY_N = 17,
  GRANIT_PHYSICAL_KEY_O = 18,
  GRANIT_PHYSICAL_KEY_P = 19,
  GRANIT_PHYSICAL_KEY_Q = 20,
  GRANIT_PHYSICAL_KEY_R = 21,
  GRANIT_PHYSICAL_KEY_S = 22,
  GRANIT_PHYSICAL_KEY_T = 23,
  GRANIT_PHYSICAL_KEY_U = 24,
  GRANIT_PHYSICAL_KEY_V = 25,
  GRANIT_PHYSICAL_KEY_W = 26,
  GRANIT_PHYSICAL_KEY_X = 27,
  GRANIT_PHYSICAL_KEY_Y = 28,
  GRANIT_PHYSICAL_KEY_Z = 29,
  GRANIT_PHYSICAL_KEY_1 = 30,
  GRANIT_PHYSICAL_KEY_2 = 31,
  GRANIT_PHYSICAL_KEY_3 = 32,
  GRANIT_PHYSICAL_KEY_4 = 33,
  GRANIT_PHYSICAL_KEY_5 = 34,
  GRANIT_PHYSICAL_KEY_6 = 35,
  GRANIT_PHYSICAL_KEY_7 = 36,
  GRANIT_PHYSICAL_KEY_8 = 37,
  GRANIT_PHYSICAL_KEY_9 = 38,
  GRANIT_PHYSICAL_KEY_0 = 39,
  GRANIT_PHYSICAL_KEY_ENTER = 40,
  GRANIT_PHYSICAL_KEY_ESCAPE = 41,
  GRANIT_PHYSICAL_KEY_BACKSPACE = 42,
  GRANIT_PHYSICAL_KEY_TAB = 43,
  GRANIT_PHYSICAL_KEY_SPACE = 44,
  GRANIT_PHYSICAL_KEY_F1 = 58,
  GRANIT_PHYSICAL_KEY_F2 = 59,
  GRANIT_PHYSICAL_KEY_F3 = 60,
  GRANIT_PHYSICAL_KEY_F4 = 61,
  GRANIT_PHYSICAL_KEY_F5 = 62,
  GRANIT_PHYSICAL_KEY_F6 = 63,
  GRANIT_PHYSICAL_KEY_F7 = 64,
  GRANIT_PHYSICAL_KEY_F8 = 65,
  GRANIT_PHYSICAL_KEY_F9 = 66,
  GRANIT_PHYSICAL_KEY_F10 = 67,
  GRANIT_PHYSICAL_KEY_F11 = 68,
  GRANIT_PHYSICAL_KEY_F12 = 69,
  GRANIT_PHYSICAL_KEY_INSERT = 73,
  GRANIT_PHYSICAL_KEY_HOME = 74,
  GRANIT_PHYSICAL_KEY_PAGE_UP = 75,
  GRANIT_PHYSICAL_KEY_DELETE = 76,
  GRANIT_PHYSICAL_KEY_END = 77,
  GRANIT_PHYSICAL_KEY_PAGE_DOWN = 78,
  GRANIT_PHYSICAL_KEY_RIGHT = 79,
  GRANIT_PHYSICAL_KEY_LEFT = 80,
  GRANIT_PHYSICAL_KEY_DOWN = 81,
  GRANIT_PHYSICAL_KEY_UP = 82,
  GRANIT_PHYSICAL_KEY_LEFT_CONTROL = 224,
  GRANIT_PHYSICAL_KEY_LEFT_SHIFT = 225,
  GRANIT_PHYSICAL_KEY_LEFT_ALT = 226,
  GRANIT_PHYSICAL_KEY_LEFT_SUPER = 227,
  GRANIT_PHYSICAL_KEY_RIGHT_CONTROL = 228,
  GRANIT_PHYSICAL_KEY_RIGHT_SHIFT = 229,
  GRANIT_PHYSICAL_KEY_RIGHT_ALT = 230,
  GRANIT_PHYSICAL_KEY_RIGHT_SUPER = 231
} granit_physical_key;

/** 不承载可打印字符的布局后逻辑键。 */
typedef enum granit_logical_key {
  GRANIT_LOGICAL_KEY_NONE = 0,
  GRANIT_LOGICAL_KEY_ENTER = 1,
  GRANIT_LOGICAL_KEY_ESCAPE = 2,
  GRANIT_LOGICAL_KEY_BACKSPACE = 3,
  GRANIT_LOGICAL_KEY_TAB = 4,
  GRANIT_LOGICAL_KEY_SPACE = 5,
  GRANIT_LOGICAL_KEY_LEFT = 6,
  GRANIT_LOGICAL_KEY_RIGHT = 7,
  GRANIT_LOGICAL_KEY_UP = 8,
  GRANIT_LOGICAL_KEY_DOWN = 9,
  GRANIT_LOGICAL_KEY_HOME = 10,
  GRANIT_LOGICAL_KEY_END = 11,
  GRANIT_LOGICAL_KEY_PAGE_UP = 12,
  GRANIT_LOGICAL_KEY_PAGE_DOWN = 13,
  GRANIT_LOGICAL_KEY_INSERT = 14,
  GRANIT_LOGICAL_KEY_DELETE = 15,
  GRANIT_LOGICAL_KEY_F1 = 16,
  GRANIT_LOGICAL_KEY_F2 = 17,
  GRANIT_LOGICAL_KEY_F3 = 18,
  GRANIT_LOGICAL_KEY_F4 = 19,
  GRANIT_LOGICAL_KEY_F5 = 20,
  GRANIT_LOGICAL_KEY_F6 = 21,
  GRANIT_LOGICAL_KEY_F7 = 22,
  GRANIT_LOGICAL_KEY_F8 = 23,
  GRANIT_LOGICAL_KEY_F9 = 24,
  GRANIT_LOGICAL_KEY_F10 = 25,
  GRANIT_LOGICAL_KEY_F11 = 26,
  GRANIT_LOGICAL_KEY_F12 = 27
} granit_logical_key;

typedef enum granit_key_action {
  GRANIT_KEY_ACTION_RELEASED = 0,
  GRANIT_KEY_ACTION_PRESSED = 1,
  GRANIT_KEY_ACTION_REPEATED = 2
} granit_key_action;

#define GRANIT_MODIFIER_LEFT_SHIFT_BIT (UINT32_C(1) << 0)
#define GRANIT_MODIFIER_RIGHT_SHIFT_BIT (UINT32_C(1) << 1)
#define GRANIT_MODIFIER_LEFT_CONTROL_BIT (UINT32_C(1) << 2)
#define GRANIT_MODIFIER_RIGHT_CONTROL_BIT (UINT32_C(1) << 3)
#define GRANIT_MODIFIER_LEFT_ALT_BIT (UINT32_C(1) << 4)
#define GRANIT_MODIFIER_RIGHT_ALT_BIT (UINT32_C(1) << 5)
#define GRANIT_MODIFIER_LEFT_SUPER_BIT (UINT32_C(1) << 6)
#define GRANIT_MODIFIER_RIGHT_SUPER_BIT (UINT32_C(1) << 7)
#define GRANIT_MODIFIER_CAPS_LOCK_BIT (UINT32_C(1) << 8)
#define GRANIT_MODIFIER_NUM_LOCK_BIT (UINT32_C(1) << 9)

#define GRANIT_POINTER_PRIMARY_BIT (UINT32_C(1) << 0)
#define GRANIT_POINTER_SECONDARY_BIT (UINT32_C(1) << 1)
#define GRANIT_POINTER_MIDDLE_BIT (UINT32_C(1) << 2)
#define GRANIT_POINTER_X1_BIT (UINT32_C(1) << 3)
#define GRANIT_POINTER_X2_BIT (UINT32_C(1) << 4)

#define GRANIT_INPUT_TEXT_CAPACITY UINT32_C(48)

typedef union granit_input_event_data {
  struct {
    uint32_t physical_key;
    uint32_t logical_key;
    uint32_t modifiers;
    uint32_t action;
  } key;
  struct {
    uint32_t length;
    char utf8[GRANIT_INPUT_TEXT_CAPACITY];
    uint8_t reserved[12];
  } text;
  struct {
    float x;
    float y;
    float delta_x;
    float delta_y;
    uint32_t buttons;
    uint32_t reserved[11];
  } pointer_moved;
  struct {
    float x;
    float y;
    uint32_t button;
    uint32_t pressed;
    uint32_t buttons;
    uint32_t reserved[11];
  } pointer_button;
  struct {
    float x;
    float y;
    float delta_x;
    float delta_y;
    uint32_t buttons;
    uint32_t reserved[11];
  } pointer_wheel;
  uint8_t reserved[64];
} granit_input_event_data;

typedef struct granit_input_event {
  uint32_t struct_size;
  uint32_t type;
  granit_window window;
  uint64_t timestamp_ns;
  granit_input_event_data data;
} granit_input_event;

#define GRANIT_INPUT_EVENT_VERSION_1_SIZE                                                          \
  ((uint32_t)(offsetof(granit_input_event, data) + sizeof(granit_input_event_data)))
#define GRANIT_INPUT_EVENT_INIT                                                                    \
  {                                                                                                \
    (uint32_t)sizeof(granit_input_event), UINT32_C(0), GRANIT_NULL_HANDLE, UINT64_C(0),            \
        {{UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)}}                                    \
  }

/** 物理键 usage 0～255 的按下状态位图。 */
typedef struct granit_keyboard_state {
  uint32_t struct_size;
  uint32_t modifiers;
  uint64_t pressed_keys[4];
  uint64_t reserved[3];
} granit_keyboard_state;

#define GRANIT_KEYBOARD_STATE_VERSION_1_SIZE ((uint32_t)(offsetof(granit_keyboard_state, reserved)))
#define GRANIT_KEYBOARD_STATE_INIT                                                                 \
  {(uint32_t)sizeof(granit_keyboard_state), UINT32_C(0), {0, 0, 0, 0}, {0, 0, 0}}

typedef struct granit_pointer_state {
  uint32_t struct_size;
  uint32_t buttons;
  float x;
  float y;
  uint32_t inside;
  uint32_t reserved[5];
} granit_pointer_state;

#define GRANIT_POINTER_STATE_VERSION_1_SIZE ((uint32_t)(offsetof(granit_pointer_state, reserved)))
#define GRANIT_POINTER_STATE_INIT                                                                  \
  {(uint32_t)sizeof(granit_pointer_state), UINT32_C(0), 0.0F, 0.0F, UINT32_C(0), {0, 0, 0, 0, 0}}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_INPUT_API granit_result granit_input_system_create(const granit_input_system_desc* desc,
                                                          granit_input_system* input_system);
GRANIT_INPUT_API granit_result granit_input_system_destroy(granit_input_system input_system);
GRANIT_INPUT_API granit_result granit_input_poll_event(granit_input_system input_system,
                                                       granit_input_event* event);
GRANIT_INPUT_API granit_result granit_input_get_keyboard_state(granit_input_system input_system,
                                                               granit_window window,
                                                               granit_keyboard_state* state);
GRANIT_INPUT_API granit_result granit_input_get_pointer_state(granit_input_system input_system,
                                                              granit_window window,
                                                              granit_pointer_state* state);

#ifdef __cplusplus
}
#endif

#endif
