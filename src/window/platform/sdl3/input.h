// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PLATFORM_SDL3_INPUT_ADAPTER_H_
#define GRANIT_PLATFORM_SDL3_INPUT_ADAPTER_H_

#include "window/input/platform_adapter.h"

namespace granit::input::detail {

void handle_sdl3_input(granit_window window, const granit_window_input_native_event& event,
                       const platform_input_sink& sink);

} // namespace granit::input::detail

#endif
