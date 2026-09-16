// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_EVENT_QUEUE_H_
#define GRANIT_WINDOW_EVENT_QUEUE_H_

#include "window/registry.h"

namespace granit::window::detail {

std::uint64_t timestamp_ns() noexcept;
std::shared_ptr<window_system_record> acquire_system(granit_window_system handle);
bool on_owner_thread(const window_system_record& system) noexcept;
void enqueue_event(const std::shared_ptr<window_system_record>& system, granit_window window,
                   std::uint32_t type);
} // namespace granit::window::detail

#endif
