// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_TIMESTAMP_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_TIMESTAMP_ADAPTER_H_

#include <memory>
#include <span>

#include "backend/contracts/resources.h"
#include "backend/webgpu/context.h"

namespace granit::detail {

class webgpu_timestamp_adapter {
public:
  webgpu_timestamp_adapter(webgpu_context& provider,
                           granit_webgpu_provider_instance instance) noexcept;

  [[nodiscard]] granit_result
  create(std::uint32_t count,
         std::unique_ptr<backend_timestamp_query_pool_resource>& pool) const noexcept;
  [[nodiscard]] granit_result read(backend_timestamp_query_pool_resource& pool, std::uint32_t first,
                                   std::span<std::uint64_t> values) const noexcept;
  [[nodiscard]] granit_webgpu_provider_timestamp_query_pool
  native_handle(backend_timestamp_query_pool_resource& pool) const noexcept;

private:
  webgpu_context* provider_{};
  granit_webgpu_provider_instance instance_{};
};

} // namespace granit::detail

#endif
