// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_texture_previews.h"

#include "model_viewer/gpu_scene.h"
#include "model_viewer/viewer_ui.h"

namespace granit::example::model_viewer {

granit::result viewer_texture_previews::rebuild(const gltf::scene& scene, gpu_scene& gpu,
                                                viewer_ui& ui) {
  clear(ui);
  const auto register_preview = [&](const gltf::texture_reference& reference, bool srgb) {
    if (reference.image == gltf::invalid_index)
      return granit::result::success;
    ImTextureID existing = ImTextureID_Invalid;
    if (find_texture_preview(reference, srgb, items_, existing))
      return granit::result::success;
    granit::texture_view_ref view;
    granit::sampler_ref sampler;
    auto result = gpu.texture_binding(reference, srgb, view, sampler);
    ImTextureID texture = ImTextureID_Invalid;
    if (result.ok())
      result = ui.register_texture(view, sampler, texture);
    if (result.ok())
      items_.push_back({reference.image, reference.sampler, srgb, texture});
    return result;
  };

  for (const auto& material : scene.materials) {
    granit::result result;
    if ((result = register_preview(material.base_color_texture, true)).failed() ||
        (result = register_preview(material.emissive_texture, true)).failed() ||
        (result = register_preview(material.metallic_roughness_texture, false)).failed() ||
        (result = register_preview(material.normal_texture, false)).failed() ||
        (result = register_preview(material.occlusion_texture, false)).failed()) {
      clear(ui);
      return result;
    }
  }
  return granit::result::success;
}

void viewer_texture_previews::clear(viewer_ui& ui) noexcept {
  for (const auto& preview : items_)
    static_cast<void>(ui.unregister_texture(preview.texture));
  items_.clear();
}

} // namespace granit::example::model_viewer
