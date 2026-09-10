// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <vector>

namespace granit::detail {

granit_result webgpu_renderer_state::upload_buffer(backend_buffer_resource& buffer,
                                                   std::uint64_t offset, const void* data,
                                                   std::uint64_t size) noexcept {
  return resources_ ? resources_->upload(buffer, offset, data, size) : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_renderer_state::upload_batch(std::span<const backend_upload_operation> uploads) noexcept {
  return resources_ ? resources_->upload_batch(uploads) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::upload_batch_async(
    std::span<const backend_upload_operation> uploads,
    std::unique_ptr<backend_upload_completion>& completion) noexcept {
  return resources_ ? resources_->upload_batch_async(uploads, completion) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::readback_batch_async(
    std::span<const backend_readback_operation> readbacks, granit_readback_layout layout,
    std::uint64_t max_result_bytes,
    std::unique_ptr<backend_readback_completion>& completion) noexcept {
  return resources_
             ? resources_->readback_batch_async(readbacks, layout, max_result_bytes, completion)
             : GRANIT_ERROR_NOT_READY;
}

} // namespace granit::detail
