// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/timestamp_adapter.h"

namespace granit::detail {
namespace {

class webgpu_timestamp_query_pool_resource final : public backend_timestamp_query_pool_resource {
public:
  webgpu_timestamp_query_pool_resource(webgpu_context& context,
                                       webgpu_instance_handle instance) noexcept
      : context_(&context), instance_(instance) {}
  ~webgpu_timestamp_query_pool_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->destroy_timestamp_query_pool(instance_, handle_));
  }

  webgpu_context* context_{};
  webgpu_instance_handle instance_{};
  webgpu_timestamp_query_pool handle_{};
};

} // namespace

webgpu_timestamp_adapter::webgpu_timestamp_adapter(webgpu_context& context,
                                                   webgpu_instance_handle instance) noexcept
    : context_(&context), instance_(instance) {}

granit_result webgpu_timestamp_adapter::create(
    std::uint32_t count,
    std::unique_ptr<backend_timestamp_query_pool_resource>& pool) const noexcept {
  try {
    auto resource = std::make_unique<webgpu_timestamp_query_pool_resource>(*context_, instance_);
    const auto result = context_->create_timestamp_query_pool(instance_, count, &resource->handle_);
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

granit_result webgpu_timestamp_adapter::read(backend_timestamp_query_pool_resource& pool,
                                             std::uint32_t first,
                                             std::span<std::uint64_t> values) const noexcept {
  const auto handle = native_handle(pool);
  return handle != 0
             ? context_->read_timestamp_query_results(instance_, handle, first, values.data(),
                                                      static_cast<std::uint32_t>(values.size()))
             : GRANIT_ERROR_INVALID_ARGUMENT;
}

webgpu_timestamp_query_pool webgpu_timestamp_adapter::native_handle(
    backend_timestamp_query_pool_resource& pool) const noexcept {
  const auto* resource = dynamic_cast<webgpu_timestamp_query_pool_resource*>(&pool);
  return resource != nullptr ? resource->handle_ : 0;
}

} // namespace granit::detail
