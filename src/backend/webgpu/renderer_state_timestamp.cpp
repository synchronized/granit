// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <vector>

namespace granit::detail {

namespace {

class webgpu_timestamp_query_pool_resource final : public backend_timestamp_query_pool_resource {
public:
  webgpu_timestamp_query_pool_resource(webgpu_device& context,
                                       webgpu_instance_handle instance) noexcept
      : device_(&context), instance_(instance) {}

  ~webgpu_timestamp_query_pool_resource() override {
    if (handle_ != 0)
      static_cast<void>(device_->destroy_timestamp_query_pool(instance_, handle_));
  }

  webgpu_device* device_{};
  webgpu_instance_handle instance_{};
  webgpu_timestamp_query_pool handle_{};
};

webgpu_timestamp_query_pool_resource*
as_timestamp_query_pool(backend_timestamp_query_pool_resource& resource) noexcept {
  return dynamic_cast<webgpu_timestamp_query_pool_resource*>(&resource);
}

} // namespace

granit_result webgpu_renderer_state::create_timestamp_query_pool(
    std::uint32_t query_count,
    std::unique_ptr<backend_timestamp_query_pool_resource>& pool) noexcept {
  if (lifecycle_.state != backend_lifecycle_state::ready)
    return GRANIT_ERROR_NOT_READY;
  try {
    auto resource = std::make_unique<webgpu_timestamp_query_pool_resource>(device_, instance_);
    const auto result =
        device_.create_timestamp_query_pool(instance_, query_count, &resource->handle_);
    if (result != GRANIT_SUCCESS)
      return result;
    pool = std::move(resource);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_renderer_state::read_timestamp_query_results(backend_timestamp_query_pool_resource& pool,
                                                    std::uint32_t first,
                                                    std::span<std::uint64_t> values) noexcept {
  const auto handle = native_timestamp_query_pool(pool);
  return handle != 0
             ? device_.read_timestamp_query_results(instance_, handle, first, values.data(),
                                                    static_cast<std::uint32_t>(values.size()))
             : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result
webgpu_renderer_state::reset_timestamp_queries(backend_command_recorder_resource& recorder,
                                               backend_timestamp_query_pool_resource& pool,
                                               std::uint32_t first, std::uint32_t count) noexcept {
  if (!command_owner_)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto command = command_native_recorder(recorder);
  const auto query = native_timestamp_query_pool(pool);
  return command != 0 && query != 0
             ? device_.recorder_reset_timestamp_queries(instance_, command, query, first, count)
             : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result webgpu_renderer_state::write_timestamp(backend_command_recorder_resource& recorder,
                                                     backend_timestamp_query_pool_resource& pool,
                                                     granit_timestamp_stage,
                                                     std::uint32_t index) noexcept {
  if (!command_owner_)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto command = command_native_recorder(recorder);
  const auto query = native_timestamp_query_pool(pool);
  return command != 0 && query != 0
             ? device_.recorder_write_timestamp(instance_, command, query, index)
             : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result
webgpu_renderer_state::set_timestamp_query_pool_name(backend_timestamp_query_pool_resource&,
                                                     std::string_view) noexcept {
  return GRANIT_SUCCESS;
}

webgpu_timestamp_query_pool webgpu_renderer_state::native_timestamp_query_pool(
    backend_timestamp_query_pool_resource& resource) const noexcept {
  const auto* pool = as_timestamp_query_pool(resource);
  return pool == nullptr ? webgpu_timestamp_query_pool{} : pool->handle_;
}

} // namespace granit::detail
