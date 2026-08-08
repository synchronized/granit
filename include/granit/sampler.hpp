// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SAMPLER_HPP_
#define GRANIT_SAMPLER_HPP_

#include <granit/resource_types.hpp>
#include <granit/result.hpp>
#include <granit/sampler.h>
#include <utility>

namespace granit {

struct sampler_desc {
  filter mag_filter{filter::linear};
  filter min_filter{filter::linear};
  mipmap_filter mip_filter{mipmap_filter::linear};
  address_mode address_u{address_mode::repeat};
  address_mode address_v{address_mode::repeat};
  address_mode address_w{address_mode::repeat};
  compare_operation compare{compare_operation::disabled};
  bool anisotropy_enabled{};
  float max_anisotropy{1.0F};
  float lod_bias{};
  float min_lod{};
  float max_lod{};
};

class sampler {
public:
  sampler() = default;
  ~sampler() { static_cast<void>(reset()); }
  sampler(const sampler&) = delete;
  sampler& operator=(const sampler&) = delete;
  sampler(sampler&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  sampler& operator=(sampler&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const sampler_desc& desc = {}) noexcept {
    if (valid() || renderer == GRANIT_NULL_HANDLE)
      return result::invalid_argument;
    const granit_sampler_desc native{.struct_size = GRANIT_SAMPLER_DESC_VERSION_1_SIZE,
                                     .mag_filter = static_cast<std::uint32_t>(desc.mag_filter),
                                     .min_filter = static_cast<std::uint32_t>(desc.min_filter),
                                     .mipmap_filter = static_cast<std::uint32_t>(desc.mip_filter),
                                     .address_mode_u = static_cast<std::uint32_t>(desc.address_u),
                                     .address_mode_v = static_cast<std::uint32_t>(desc.address_v),
                                     .address_mode_w = static_cast<std::uint32_t>(desc.address_w),
                                     .compare_operation = static_cast<std::uint32_t>(desc.compare),
                                     .anisotropy_enabled =
                                         desc.anisotropy_enabled ? UINT32_C(1) : UINT32_C(0),
                                     .max_anisotropy = desc.max_anisotropy,
                                     .lod_bias = desc.lod_bias,
                                     .min_lod = desc.min_lod,
                                     .max_lod = desc.max_lod,
                                     .reserved = 0};
    const auto value = granit_sampler_create(renderer, &native, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_sampler_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_sampler native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_sampler handle_{GRANIT_NULL_HANDLE};
};
} // namespace granit
#endif
