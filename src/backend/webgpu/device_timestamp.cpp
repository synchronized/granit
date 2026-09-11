// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device.h"
#include "backend/webgpu/device_state.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>

#include <webgpu/webgpu.h>

namespace {

using granit::detail::webgpu_device_state;
using namespace granit::detail::webgpu_native;

struct timestamp_map_request {
  std::shared_ptr<webgpu_device_state::timestamp_query_record> query;
};

void receive_timestamp_map(WGPUMapAsyncStatus status, WGPUStringView, void* data, void*) noexcept {
  std::unique_ptr<timestamp_map_request> request{static_cast<timestamp_map_request*>(data)};
  auto& query = *request->query;
  if (status != WGPUMapAsyncStatus_Success) {
    query.map_state.store(3, std::memory_order_release);
    return;
  }
  const auto size = static_cast<std::size_t>(query.count) * sizeof(std::uint64_t);
  const auto* mapped = wgpuBufferGetConstMappedRange(query.read_buffer, 0, size);
  if (mapped == nullptr) {
    query.map_state.store(3, std::memory_order_release);
    return;
  }
  std::memcpy(query.values.data(), mapped, size);
  wgpuBufferUnmap(query.read_buffer);
  query.map_state.store(2, std::memory_order_release);
}

granit_result create_timestamp_query_pool(webgpu_instance_handle instance,
                                          std::uint32_t query_count,
                                          webgpu_timestamp_query_pool* out_pool) noexcept {
  if (out_pool != nullptr)
    *out_pool = 0;
  if (instance == 0 || query_count == 0 || out_pool == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  if (const auto ready = require_ready(state); ready != GRANIT_SUCCESS)
    return ready;
  if ((state.capabilities.renderer_features & GRANIT_WEBGPU_FEATURE_TIMESTAMP_QUERY_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;

  std::shared_ptr<webgpu_device_state::timestamp_query_record> query;
  try {
    query = std::make_shared<webgpu_device_state::timestamp_query_record>();
    query->values.resize(query_count);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }

  WGPUQuerySetDescriptor query_desc = WGPU_QUERY_SET_DESCRIPTOR_INIT;
  query_desc.type = WGPUQueryType_Timestamp;
  query_desc.count = query_count;
  const auto query_set = wgpuDeviceCreateQuerySet(state.device, &query_desc);
  if (query_set == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto size = static_cast<std::uint64_t>(query_count) * sizeof(std::uint64_t);
  WGPUBufferDescriptor resolve_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
  resolve_desc.size = size;
  resolve_desc.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
  const auto resolve_buffer = wgpuDeviceCreateBuffer(state.device, &resolve_desc);
  WGPUBufferDescriptor read_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
  read_desc.size = size;
  read_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
  const auto read_buffer = wgpuDeviceCreateBuffer(state.device, &read_desc);
  if (resolve_buffer == nullptr || read_buffer == nullptr) {
    if (read_buffer != nullptr)
      wgpuBufferRelease(read_buffer);
    if (resolve_buffer != nullptr)
      wgpuBufferRelease(resolve_buffer);
    wgpuQuerySetRelease(query_set);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  query->query_set = query_set;
  query->resolve_buffer = resolve_buffer;
  query->read_buffer = read_buffer;
  query->count = query_count;
  try {
    const auto handle = next_handle<webgpu_timestamp_query_pool>(next_timestamp_query_pool);
    if (!state.timestamp_queries.emplace(handle, std::move(query)).second)
      throw std::bad_alloc{};
    *out_pool = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_timestamp_query_pool(webgpu_instance_handle instance,
                                           webgpu_timestamp_query_pool pool) noexcept {
  if (instance == 0 || pool == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto query = found->second->timestamp_queries.find(pool);
  if (query == found->second->timestamp_queries.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (query->second->map_state.load(std::memory_order_acquire) == 1)
    return GRANIT_ERROR_NOT_READY;
  found->second->timestamp_queries.erase(query);
  return GRANIT_SUCCESS;
}

granit_result recorder_reset_timestamp_queries(webgpu_instance_handle instance,
                                               webgpu_command_recorder recorder,
                                               webgpu_timestamp_query_pool pool,
                                               std::uint32_t first, std::uint32_t count) noexcept {
  if (instance == 0 || recorder == 0 || pool == 0 || count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto query = state.timestamp_queries.find(pool);
  if (command == state.command_recorders.end() || query == state.timestamp_queries.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || first > query->second->count ||
      count > query->second->count - first)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (query->second->map_state.load(std::memory_order_acquire) == 1)
    return GRANIT_ERROR_NOT_READY;
  query->second->map_state.store(0, std::memory_order_release);
  if (std::find(command->second.timestamp_pools.begin(), command->second.timestamp_pools.end(),
                pool) == command->second.timestamp_pools.end())
    command->second.timestamp_pools.push_back(pool);
  return GRANIT_SUCCESS;
}

granit_result recorder_write_timestamp(webgpu_instance_handle instance,
                                       webgpu_command_recorder recorder,
                                       webgpu_timestamp_query_pool pool,
                                       std::uint32_t query_index) noexcept {
  if (instance == 0 || recorder == 0 || pool == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto query = state.timestamp_queries.find(pool);
  if (command == state.command_recorders.end() || query == state.timestamp_queries.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.pass != nullptr ||
      command->second.compute_pass != nullptr || query_index >= query->second->count)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuCommandEncoderWriteTimestamp(command->second.encoder, query->second->query_set, query_index);
  if (std::find(command->second.timestamp_pools.begin(), command->second.timestamp_pools.end(),
                pool) == command->second.timestamp_pools.end())
    command->second.timestamp_pools.push_back(pool);
  return GRANIT_SUCCESS;
}

granit_result read_timestamp_query_results(webgpu_instance_handle instance,
                                           webgpu_timestamp_query_pool pool, std::uint32_t first,
                                           std::uint64_t* values, std::uint32_t count) noexcept {
  if (instance == 0 || pool == 0 || values == nullptr || count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::shared_ptr<webgpu_device_state::timestamp_query_record> query;
  {
    const std::scoped_lock lock{instances_mutex};
    const auto found = instances.find(instance);
    if (found == instances.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto query_found = found->second->timestamp_queries.find(pool);
    if (query_found == found->second->timestamp_queries.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    query = query_found->second;
  }
  if (first > query->count || count > query->count - first)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto state = query->map_state.load(std::memory_order_acquire);
  if (state == 0) {
    auto* request = new (std::nothrow) timestamp_map_request{query};
    if (request == nullptr)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    query->map_state.store(1, std::memory_order_release);
#if defined(__EMSCRIPTEN__)
    constexpr auto callback_mode = WGPUCallbackMode_AllowSpontaneous;
#else
    constexpr auto callback_mode = WGPUCallbackMode_AllowProcessEvents;
#endif
    const WGPUBufferMapCallbackInfo callback{nullptr, callback_mode, receive_timestamp_map, request,
                                             nullptr};
    static_cast<void>(wgpuBufferMapAsync(
        query->read_buffer, WGPUMapMode_Read, 0,
        static_cast<std::size_t>(query->count) * sizeof(std::uint64_t), callback));
    return GRANIT_ERROR_NOT_READY;
  }
  if (state == 1)
    return GRANIT_ERROR_NOT_READY;
  if (state == 3)
    return GRANIT_ERROR_INTERNAL;
  std::copy_n(query->values.data() + first, count, values);
  return GRANIT_SUCCESS;
}

} // namespace

namespace granit::detail {

granit_result
webgpu_device::create_timestamp_query_pool(std::uint32_t count,
                                           webgpu_timestamp_query_pool* pool) noexcept {
  return open_ ? ::create_timestamp_query_pool(instance_, count, pool) : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_device::destroy_timestamp_query_pool(webgpu_timestamp_query_pool pool) noexcept {
  return open_ ? ::destroy_timestamp_query_pool(instance_, pool) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_device::recorder_reset_timestamp_queries(webgpu_command_recorder recorder,
                                                              webgpu_timestamp_query_pool pool,
                                                              std::uint32_t first,
                                                              std::uint32_t count) noexcept {
  return open_ ? ::recorder_reset_timestamp_queries(instance_, recorder, pool, first, count)
               : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_device::recorder_write_timestamp(webgpu_command_recorder recorder,
                                                      webgpu_timestamp_query_pool pool,
                                                      std::uint32_t index) noexcept {
  return open_ ? ::recorder_write_timestamp(instance_, recorder, pool, index)
               : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_device::read_timestamp_query_results(webgpu_timestamp_query_pool pool,
                                                          std::uint32_t first,
                                                          std::uint64_t* values,
                                                          std::uint32_t count) noexcept {
  return open_ ? ::read_timestamp_query_results(instance_, pool, first, values, count)
               : GRANIT_ERROR_NOT_READY;
}

} // namespace granit::detail
