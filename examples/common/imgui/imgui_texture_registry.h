// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_TEXTURE_REGISTRY_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_TEXTURE_REGISTRY_H_

#include <imgui.h>

#include <granit/core/result.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.hpp>

#include <cstdint>
#include <vector>

namespace granit::example::imgui {

/**
 * 将 ImGui 的整数 Texture ID 映射到仍存活的 Granit 资源。
 *
 * 注册、注销和解析均由 UI/渲染线程串行调用；ID 包含 generation，不保存或解引用指针。
 */
class texture_registry {
public:
  [[nodiscard]] granit::result register_texture(granit::texture_view_ref view,
                                                granit::sampler_ref sampler,
                                                ImTextureID& texture);
  [[nodiscard]] granit::result unregister_texture(ImTextureID texture) noexcept;
  void clear() noexcept;

  [[nodiscard]] granit::result resolve(ImTextureID texture,
                                       granit::canvas_draw_state& state) const noexcept;
  [[nodiscard]] static granit::result resolver(ImTextureID texture,
                                               granit::canvas_draw_state& state,
                                               void* user_data) noexcept;

private:
  struct slot {
    granit::texture_view_ref view;
    granit::sampler_ref sampler;
    std::uint32_t generation{1};
    bool alive{};
  };

  static void retire(slot& target) noexcept;

  std::vector<slot> slots_;
};

} // namespace granit::example::imgui

#endif
