// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_PBR_DEFAULT_RESOURCES_H
#define GRANIT_MATERIAL_PBR_DEFAULT_RESOURCES_H

#include "material/material_gpu_instance.h"

#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.hpp>

#include <array>

namespace granit::material {

/** 拥有 PBR 缺省纹理和共享采样器，生命周期必须覆盖引用它们的材质实例。 */
class pbr_default_resources {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] granit_result bind(material_gpu_instance& instance) const noexcept;
  [[nodiscard]] bool initialized() const noexcept { return sampler_.valid(); }

private:
  std::array<granit::texture, 5> textures_;
  std::array<granit::texture_view, 5> views_;
  granit::sampler sampler_;
};

} // namespace granit::material

#endif
