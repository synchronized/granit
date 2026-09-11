// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <vector>

namespace granit::detail {

granit_result
webgpu_renderer_state::submit_command_recorder(backend_command_recorder_resource& recorder,
                                               submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (!capabilities_initialized_)
    return GRANIT_ERROR_NOT_READY;
  const auto result = command_submit(recorder);
  if (result == GRANIT_SUCCESS)
    submitted_serial = next_submission_serial_++;
  return result;
}

granit_result webgpu_renderer_state::submit_command_recorders(
    std::span<backend_command_recorder_resource* const> recorders,
    submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (recorders.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (auto* recorder : recorders) {
    if (recorder == nullptr)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto result = command_submit(*recorder);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  submitted_serial = next_submission_serial_++;
  return GRANIT_SUCCESS;
}

granit_result
webgpu_renderer_state::wait_command_recorder(backend_command_recorder_resource&) noexcept {
  return GRANIT_SUCCESS;
}

granit_result webgpu_renderer_state::wait_for_all_submissions() noexcept {
  return lifecycle_.state == backend_lifecycle_state::device_lost ? GRANIT_ERROR_DEVICE_LOST
                                                                  : GRANIT_SUCCESS;
}

void webgpu_renderer_state::retire_resource(submission_serial, retirement_order,
                                            std::shared_ptr<void> resource) {
  // WebGPU 命令会持有所引用对象；命令完成编码后即可释放应用侧引用。
  resource.reset();
}

std::size_t webgpu_renderer_state::collect_retired() noexcept { return 0; }

std::size_t webgpu_renderer_state::pending_retirement_count() const noexcept { return 0; }

} // namespace granit::detail
