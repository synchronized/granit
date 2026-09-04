// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/frame_executor.h"

#include <utility>

namespace granit::example::model_viewer {

inline_frame_executor::inline_frame_executor(frame_execute_callback callback,
                                             void* user_data) noexcept
    : callback_(callback), user_data_(user_data) {}

granit::result inline_frame_executor::submit(frame_packet packet,
                                             frame_execution_result& output) {
  output = {};
  if (callback_ == nullptr)
    return granit::result::invalid_argument;
  return callback_(std::move(packet), output, user_data_);
}

granit::result inline_frame_executor::flush() noexcept {
  return granit::result::success;
}

} // namespace granit::example::model_viewer
