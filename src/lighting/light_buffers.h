// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_LIGHT_BUFFERS_H
#define GRANIT_LIGHTING_LIGHT_BUFFERS_H

#include "lighting/light_data.h"

#include <granit/renderer/buffer.hpp>

namespace granit::lighting {

inline constexpr std::uint32_t light_binding_counts = 8;
inline constexpr std::uint32_t light_binding_directional = 9;
inline constexpr std::uint32_t light_binding_point = 10;
inline constexpr std::uint32_t light_binding_spot = 11;

struct alignas(16) gpu_light_counts {
  std::uint32_t directional = 0;
  std::uint32_t point = 0;
  std::uint32_t spot = 0;
  std::uint32_t padding = 0;
};

static_assert(sizeof(gpu_light_counts) == 16);

/**
 * 拥有逐 View 光源 GPU Buffer，不创建 Bind Group。
 *
 * 上层组合资源负责把 binding 8～11 与可选阴影、IBL 资源合并到同一个 Group 3。
 */
class light_buffers {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer,
                                         const light_limits& capacities) noexcept;
  [[nodiscard]] granit_result update(const packed_view_lights& lights) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return counts_.valid(); }
  [[nodiscard]] const light_limits& capacities() const noexcept { return capacities_; }
  [[nodiscard]] granit_buffer counts() const noexcept { return counts_.native_handle(); }
  [[nodiscard]] granit_buffer directional() const noexcept { return directional_.native_handle(); }
  [[nodiscard]] granit_buffer point() const noexcept { return point_.native_handle(); }
  [[nodiscard]] granit_buffer spot() const noexcept { return spot_.native_handle(); }

private:
  light_limits capacities_{};
  granit::buffer counts_;
  granit::buffer directional_;
  granit::buffer point_;
  granit::buffer spot_;
};

} // namespace granit::lighting

#endif
