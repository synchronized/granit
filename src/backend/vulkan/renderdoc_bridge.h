// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_RENDERDOC_BRIDGE_H_
#define GRANIT_BACKEND_VULKAN_RENDERDOC_BRIDGE_H_

#include <cstdint>

namespace granit::detail {

class diagnostic_sink;

/** RenderDoc 可选运行时桥接；不拥有公共 API，也不依赖 RenderDoc 头文件。 */
class renderdoc_bridge final {
public:
  enum class mode { off, trigger, frame };

  renderdoc_bridge() = default;
  ~renderdoc_bridge();

  renderdoc_bridge(const renderdoc_bridge&) = delete;
  renderdoc_bridge& operator=(const renderdoc_bridge&) = delete;

  void initialize(const diagnostic_sink& diagnostics) noexcept;
  void trigger_initial_capture() noexcept;
  void notify_frame_boundary() noexcept;

private:
  void reset() noexcept;
  void emit_status(const diagnostic_sink& diagnostics, const char* message) const noexcept;

  mode mode_{mode::off};
  std::uint64_t target_frame_{};
  std::uint64_t presented_frames_{};
  bool triggered_{};
  bool available_{};
  void* module_{};
  void* api_{};
};

} // namespace granit::detail

#endif
