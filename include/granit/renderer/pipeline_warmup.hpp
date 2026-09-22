// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_WARMUP_HPP_
#define GRANIT_PIPELINE_WARMUP_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

#include <granit/core/result.hpp>
#include <granit/renderer/async_operation.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/pipeline_warmup.h>
#include <granit/renderer/renderer.hpp>

namespace granit {

class pipeline_warmup_batch;

/** 不拥有 Pipeline 预热批次，只在来源批次的有效期内使用。 */
class pipeline_warmup_batch_ref {
public:
  pipeline_warmup_batch_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_pipeline_warmup_batch native_handle() const noexcept {
    return handle_;
  }
  [[nodiscard]] static constexpr pipeline_warmup_batch_ref
  from_native(granit_pipeline_warmup_batch handle) noexcept {
    return pipeline_warmup_batch_ref{handle};
  }

private:
  friend class pipeline_warmup_batch;

  explicit constexpr pipeline_warmup_batch_ref(granit_pipeline_warmup_batch handle) noexcept
      : handle_(handle) {}

  granit_pipeline_warmup_batch handle_{GRANIT_NULL_HANDLE};
};

enum class pipeline_warmup_type : std::uint32_t {
  graphics = GRANIT_PIPELINE_WARMUP_TYPE_GRAPHICS,
  compute = GRANIT_PIPELINE_WARMUP_TYPE_COMPUTE,
};

struct pipeline_warmup_batch_options {
  std::uint32_t max_operation_count{};
};

struct pipeline_warmup_batch_info {
  std::uint32_t operation_count{};
  std::uint32_t max_operation_count{};
};

struct pipeline_warmup_result_info {
  pipeline_warmup_type type{pipeline_warmup_type::graphics};
  result operation_result{result::not_ready};
  bool cache_hit{};
  std::array<std::byte, GRANIT_PIPELINE_WARMUP_CACHE_KEY_SIZE> cache_key{};
};

class pipeline_warmup_batch {
public:
  pipeline_warmup_batch() = default;
  ~pipeline_warmup_batch() { static_cast<void>(reset_handle()); }
  pipeline_warmup_batch(const pipeline_warmup_batch&) = delete;
  pipeline_warmup_batch& operator=(const pipeline_warmup_batch&) = delete;
  pipeline_warmup_batch(pipeline_warmup_batch&& other) noexcept
      : renderer_(other.renderer_), handle_(other.handle_) {
    other.renderer_ = GRANIT_NULL_HANDLE;
    other.handle_ = GRANIT_NULL_HANDLE;
  }
  pipeline_warmup_batch& operator=(pipeline_warmup_batch&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset_handle());
      renderer_ = other.renderer_;
      handle_ = other.handle_;
      other.renderer_ = GRANIT_NULL_HANDLE;
      other.handle_ = GRANIT_NULL_HANDLE;
    }
    return *this;
  }

  [[nodiscard]] result create(renderer_ref owner,
                              const pipeline_warmup_batch_options& options = {}) noexcept {
    const auto renderer = owner.native_handle();
    static_cast<void>(reset_handle());
    const granit_pipeline_warmup_batch_desc desc{
        GRANIT_PIPELINE_WARMUP_BATCH_DESC_VERSION_1_SIZE, 0, options.max_operation_count, 0};
    const auto value = granit_pipeline_warmup_batch_create(renderer, &desc, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }
  [[nodiscard]] result create(renderer& owner,
                              const pipeline_warmup_batch_options& options = {}) noexcept {
    return create(owner.ref(), options);
  }
  [[nodiscard]] result add_graphics(const graphics_pipeline_desc& desc,
                                    std::uint32_t& index) noexcept {
    return detail::with_native_graphics_pipeline_desc(desc, [&](const auto& native) {
      return granit_pipeline_warmup_batch_add_graphics(renderer_, handle_, &native, &index);
    });
  }
  [[nodiscard]] result add_compute(const compute_pipeline_desc& desc,
                                   std::uint32_t& index) noexcept {
    const auto native = detail::to_native(desc);
    return from_native(
        granit_pipeline_warmup_batch_add_compute(renderer_, handle_, &native, &index));
  }
  [[nodiscard]] result get_info(pipeline_warmup_batch_info& info) const noexcept {
    granit_pipeline_warmup_batch_info native = GRANIT_PIPELINE_WARMUP_BATCH_INFO_INIT;
    const auto value = granit_pipeline_warmup_batch_get_info(renderer_, handle_, &native);
    if (value == GRANIT_SUCCESS)
      info = {native.operation_count, native.max_operation_count};
    return from_native(value);
  }
  [[nodiscard]] result submit_async(async_operation& operation) noexcept {
    static_cast<void>(operation.reset());
    granit_async_operation native{};
    const auto value = granit_pipeline_warmup_batch_submit_async(renderer_, handle_, &native);
    if (value == GRANIT_SUCCESS)
      operation = async_operation{renderer_ref::from_native(renderer_), native};
    return from_native(value);
  }
  [[nodiscard]] result reset() noexcept {
    return from_native(granit_pipeline_warmup_batch_reset(renderer_, handle_));
  }
  [[nodiscard]] result reset_handle() noexcept {
    if (handle_ == GRANIT_NULL_HANDLE)
      return result::success;
    const auto renderer = renderer_;
    const auto handle = handle_;
    renderer_ = GRANIT_NULL_HANDLE;
    handle_ = GRANIT_NULL_HANDLE;
    return from_native(granit_pipeline_warmup_batch_destroy(renderer, handle));
  }
  [[nodiscard]] granit_pipeline_warmup_batch native_handle() const noexcept { return handle_; }
  [[nodiscard]] constexpr pipeline_warmup_batch_ref ref() const noexcept {
    return pipeline_warmup_batch_ref{handle_};
  }

private:
  granit_renderer renderer_{};
  granit_pipeline_warmup_batch handle_{};
};

[[nodiscard]] inline result
get_pipeline_warmup_result(const async_operation& operation, std::uint32_t index,
                           pipeline_warmup_result_info& info) noexcept {
  granit_pipeline_warmup_result_info native = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;
  const auto value = granit_pipeline_warmup_operation_get_result(
      operation.native_renderer(), operation.native_handle(), index, &native);
  if (value == GRANIT_SUCCESS) {
    info.type = static_cast<pipeline_warmup_type>(native.type);
    info.operation_result = from_native(native.result);
    info.cache_hit = native.cache_hit != 0;
    for (std::size_t offset = 0; offset < info.cache_key.size(); ++offset)
      info.cache_key[offset] = static_cast<std::byte>(native.cache_key[offset]);
  }
  return from_native(value);
}

} // namespace granit

#endif
