// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_SCENE_UPLOAD_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_SCENE_UPLOAD_H_

#include <cstdint>

namespace granit::example::gltf_rendering {

enum class scene_upload_stage {
  planning,
  geometry,
  textures,
  samplers,
  meshes,
  materials,
};

struct scene_upload_progress {
  scene_upload_stage stage{scene_upload_stage::planning};
  std::uint32_t completed{};
  std::uint32_t total{};
};

/** 返回 false 可在资源边界取消上传；正在执行的单次后端提交不会被中断。 */
using scene_upload_callback = bool (*)(const scene_upload_progress& progress, void* user_data);

} // namespace granit::example::gltf_rendering

#endif // GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_SCENE_UPLOAD_H_
