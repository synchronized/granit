// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MATERIAL_HPP_
#define GRANIT_PIPELINE_MATERIAL_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/material.h>

#include <limits>
#include <span>
#include <string_view>
#include <utility>

namespace granit {

[[nodiscard]] inline std::uint64_t material_parameter_id(std::string_view name) noexcept {
  if (name.size() > std::numeric_limits<std::uint32_t>::max())
    return 0;
  return granit_material_parameter_id(name.data(), static_cast<std::uint32_t>(name.size()));
}

/** 公共 Material C ABI 的轻量 move-only RAII 包装。 */
class material_instance {
public:
  material_instance() = default;
  ~material_instance() { static_cast<void>(reset()); }
  material_instance(const material_instance&) = delete;
  material_instance& operator=(const material_instance&) = delete;
  material_instance(material_instance&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  material_instance& operator=(material_instance&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_material_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_material_create(renderer, &desc, &handle_));
    if (succeeded(value))
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result update(std::span<const granit_material_parameter_update> updates) noexcept {
    if (updates.size() > std::numeric_limits<std::uint32_t>::max())
      return result::invalid_argument;
    return from_native(granit_material_update(renderer_, handle_, updates.data(),
                                              static_cast<std::uint32_t>(updates.size())));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    return from_native(granit_material_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_material native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_material handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
