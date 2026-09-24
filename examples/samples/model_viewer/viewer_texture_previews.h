// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_TEXTURE_PREVIEWS_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_TEXTURE_PREVIEWS_H_

#include "model_viewer/viewer_panels.h"

#include <granit/core/result.hpp>

#include <span>
#include <vector>

namespace granit::example::model_viewer {

class gpu_scene;
class viewer_ui;

/** 管理 Viewer 面板使用的去重材质纹理注册。 */
class viewer_texture_previews final {
public:
  [[nodiscard]] granit::result rebuild(const gltf::scene& scene, gpu_scene& gpu, viewer_ui& ui);
  void clear(viewer_ui& ui) noexcept;

  [[nodiscard]] std::span<const texture_preview> items() const noexcept { return items_; }

private:
  std::vector<texture_preview> items_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_VIEWER_TEXTURE_PREVIEWS_H_
