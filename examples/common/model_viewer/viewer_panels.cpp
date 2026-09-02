// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_panels.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <vector>

namespace granit::example::model_viewer {
namespace {

const char* node_name(const gltf::node& node) noexcept {
  return node.name.empty() ? "Unnamed Node" : node.name.c_str();
}

const char* material_name(const gltf::material& material) noexcept {
  return material.name.empty() ? "Unnamed Material" : material.name.c_str();
}

const char* filter_name(std::uint32_t filter) noexcept {
  return filter == 9728 || filter == 9984 || filter == 9986 ? "Nearest" : "Linear";
}

const char* wrap_name(std::uint32_t wrap) noexcept {
  if (wrap == 33071)
    return "Clamp";
  if (wrap == 33648)
    return "Mirror";
  return "Repeat";
}

void draw_texture_preview(const char* label, const gltf::texture_reference& reference, bool srgb,
                          const gltf::scene& scene, std::span<const texture_preview> previews) {
  ImGui::PushID(label);
  ImGui::TextUnformatted(label);
  ImTextureID texture = ImTextureID_Invalid;
  if (find_texture_preview(reference, srgb, previews, texture))
    ImGui::Image(ImTextureRef{texture}, {64, 64});
  else
    ImGui::TextDisabled("No texture");
  if (reference.image != gltf::invalid_index)
    ImGui::Text("Image: %u (%s)", reference.image, srgb ? "sRGB" : "Linear");
  if (reference.sampler < scene.samplers.size()) {
    const auto& sampler = scene.samplers[reference.sampler];
    ImGui::Text("Sampler: mag %s, min %s, U %s, V %s", filter_name(sampler.mag_filter),
                filter_name(sampler.min_filter), wrap_name(sampler.wrap_u),
                wrap_name(sampler.wrap_v));
  } else {
    ImGui::TextDisabled("Sampler: default");
  }
  ImGui::PopID();
}

void draw_scene_node(const gltf::scene& scene, std::uint32_t index, const viewer_state& state,
                     viewer_change& change, std::vector<bool>& visited) {
  if (index >= scene.nodes.size() || visited[index])
    return;
  visited[index] = true;
  const auto& node = scene.nodes[index];
  ImGui::PushID(&node);
  auto visible = state.node_visible(index);
  if (ImGui::Checkbox("##visible", &visible)) {
    change.visibility_node = index;
    change.visible = visible;
  }
  ImGui::SameLine();
  auto flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
  if (node.children.empty())
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  if (state.selected_node() == index)
    flags |= ImGuiTreeNodeFlags_Selected;
  const auto open = ImGui::TreeNodeEx("##node", flags, "%s", node_name(node));
  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    change.selected_node = index;
  if (open && !node.children.empty()) {
    for (const auto child : node.children)
      draw_scene_node(scene, child, state, change, visited);
    ImGui::TreePop();
  }
  ImGui::PopID();
}

void draw_scene_panel(const gltf::scene& scene, const viewer_state& state, viewer_change& change) {
  std::vector<bool> visited(scene.nodes.size());
  for (const auto root : scene.roots)
    draw_scene_node(scene, root, state, change, visited);
  // Loader 通常会给出 Roots；仍显示未挂接 Node，便于诊断不完整的测试资产。
  for (std::uint32_t index = 0; index < scene.nodes.size(); ++index)
    draw_scene_node(scene, index, state, change, visited);
}

void draw_inspector_panel(const gltf::scene& scene, const viewer_state& state,
                          std::span<const texture_preview> previews,
                          viewer_panel_changes& changes) {
  if (state.selected_node() >= scene.nodes.size()) {
    ImGui::TextUnformatted("No node selected");
  } else {
    const auto& node = scene.nodes[state.selected_node()];
    ImGui::Text("Node: %s", node_name(node));
    ImGui::Text("Mesh: %s", node.mesh == gltf::invalid_index ? "None" : "Assigned");
  }

  const auto preview = state.selected_material() < scene.materials.size()
                           ? material_name(scene.materials[state.selected_material()])
                           : "None";
  if (ImGui::BeginCombo("Material", preview)) {
    if (ImGui::Selectable("None", state.selected_material() == gltf::invalid_index))
      changes.state.selected_material = gltf::invalid_index;
    for (std::uint32_t index = 0; index < scene.materials.size(); ++index) {
      if (ImGui::Selectable(material_name(scene.materials[index]),
                            state.selected_material() == index))
        changes.state.selected_material = index;
    }
    ImGui::EndCombo();
  }

  if (state.selected_material() < scene.materials.size()) {
    const auto& material = scene.materials[state.selected_material()];
    material_factor_edit edit{.base_color = material.base_color,
                              .metallic = material.metallic,
                              .roughness = material.roughness,
                              .normal_scale = material.normal_scale,
                              .occlusion_strength = material.occlusion_strength,
                              .emissive = material.emissive};
    bool edited = ImGui::ColorEdit4("Base Color", &edit.base_color.x);
    edited |= ImGui::SliderFloat("Metallic", &edit.metallic, 0.0F, 1.0F);
    edited |= ImGui::SliderFloat("Roughness", &edit.roughness, 0.0F, 1.0F);
    edited |= ImGui::SliderFloat("Normal Scale", &edit.normal_scale, 0.0F, 10.0F);
    edited |= ImGui::SliderFloat("Occlusion", &edit.occlusion_strength, 0.0F, 1.0F);
    edited |= ImGui::ColorEdit3("Emissive", &edit.emissive.x,
                                ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
    if (edited)
      changes.material = edit;
    ImGui::SeparatorText("Textures");
    draw_texture_preview("Base Color", material.base_color_texture, true, scene, previews);
    draw_texture_preview("Metallic / Roughness", material.metallic_roughness_texture, false, scene,
                         previews);
    draw_texture_preview("Normal", material.normal_texture, false, scene, previews);
    draw_texture_preview("Occlusion", material.occlusion_texture, false, scene, previews);
    draw_texture_preview("Emissive", material.emissive_texture, true, scene, previews);
  }
}

void draw_lighting_panel(const viewer_state& state, viewer_change& change) {
  // ImGui 默认控件宽度会与较长标签争用同一行，窄侧栏中会截断环境光字段名。
  // 统一为最长标签预留空间，使窗口缩放后仍能完整辨认控件含义。
  const auto label_width = ImGui::CalcTextSize("Environment Rotation").x;
  ImGui::PushItemWidth(-label_width - ImGui::GetStyle().ItemInnerSpacing.x);
  auto light = state.directional_light();
  auto exposure = state.exposure_ev();
  bool changed = ImGui::DragFloat3("Direction", &light.direction.x, 0.01F, -1.0F, 1.0F);
  changed |= ImGui::DragFloat3("Radiance", &light.radiance.x, 0.05F, 0.0F, 100000.0F);
  if (changed)
    change.directional_light = light;
  if (ImGui::SliderFloat("Exposure EV", &exposure, -24.0F, 24.0F))
    change.exposure_ev = exposure;
  auto environment_intensity = state.environment_intensity();
  if (ImGui::SliderFloat("Environment Intensity", &environment_intensity, 0.0F, 8.0F))
    change.environment_intensity = environment_intensity;
  auto environment_rotation = state.environment_rotation_radians();
  if (ImGui::SliderAngle("Environment Rotation", &environment_rotation, -180.0F, 180.0F))
    change.environment_rotation_radians = environment_rotation;
  auto background = state.background_color();
  if (ImGui::ColorEdit3("Background", &background.x))
    change.background_color = background;

  static constexpr std::array modes{"Shaded",          "Base Color",       "Normals",
                                    "Metallic",        "Roughness",        "Geometric Normals",
                                    "Sampled Normals", "Vertex Normals",   "Vertex Tangents"};
  auto mode = static_cast<int>(state.debug_display());
  if (ImGui::Combo("Debug Display", &mode, modes.data(), static_cast<int>(modes.size())))
    change.debug_display = static_cast<debug_display_mode>(mode);
  ImGui::PopItemWidth();
}

void draw_renderer_panel(const renderer_panel_info& info) {
  ImGui::Text("Backend: %.*s", static_cast<int>(info.backend.size()), info.backend.data());
  ImGui::Text("Adapter: %.*s", static_cast<int>(info.adapter.size()), info.adapter.data());
  ImGui::Text("Swapchain: %u x %u, %.*s", info.width, info.height,
              static_cast<int>(info.swapchain_format.size()), info.swapchain_format.data());
  ImGui::Text("Present: %.*s", static_cast<int>(info.present_mode.size()),
              info.present_mode.data());
  ImGui::Text("Frame slots: %u", info.frame_slots);
}

void draw_performance_panel(const performance_panel_info& info) {
  ImGui::Text("FPS: %.1f", info.frames_per_second);
  ImGui::Text("CPU frame: %.3f ms", info.cpu_frame_ms);
  ImGui::Text("Frame-slot wait: %.3f ms", info.frame_slot_wait_ms);
  ImGui::Text("Present wait: %.3f ms", info.present_wait_ms);
  if (info.gpu_timing_available)
    ImGui::Text("GPU frame: %.3f ms", info.gpu_frame_ms);
  else
    ImGui::TextUnformatted("GPU frame: unavailable");
  const auto draw_summary = [](const char* label, const metric_summary& summary) {
    if (summary.sample_count == 0) {
      ImGui::TextDisabled("%s: unavailable", label);
      return;
    }
    ImGui::Text("%s p50 %.3f | p95 %.3f | max %.3f", label, summary.p50, summary.p95,
                summary.maximum);
  };
  ImGui::SeparatorText("240-frame history");
  draw_summary("CPU ms", info.history.cpu_frame_ms);
  draw_summary("Slot wait ms", info.history.frame_slot_wait_ms);
  draw_summary("Present wait ms", info.history.present_wait_ms);
  draw_summary("GPU ms", info.history.gpu_frame_ms);
}

struct panel_placement {
  ImVec2 position;
  ImVec2 size;
};

template <typename Callback>
void draw_panel(const char* name, bool& open, panel_placement placement, Callback&& callback) {
  if (!open)
    return;
  ImGui::SetNextWindowPos(placement.position, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(placement.size, ImGuiCond_FirstUseEver);
  if (ImGui::Begin(name, &open))
    callback();
  ImGui::End();
}

struct viewer_panel_layout {
  panel_placement scene;
  panel_placement inspector;
  panel_placement lighting;
  panel_placement renderer;
  panel_placement performance;
};

viewer_panel_layout default_panel_layout() noexcept {
  const auto* viewport = ImGui::GetMainViewport();
  const auto origin = viewport->WorkPos;
  const auto extent = viewport->WorkSize;
  constexpr float gap = 8.0F;
  const auto left_width = std::clamp(extent.x * 0.24F, 220.0F, 320.0F);
  const auto right_width = std::clamp(extent.x * 0.29F, 280.0F, 380.0F);
  const auto scene_height = std::clamp(extent.y * 0.34F, 160.0F, 260.0F);
  const auto renderer_height = std::clamp(extent.y * 0.22F, 130.0F, 170.0F);
  const auto performance_height = std::clamp(extent.y * 0.34F, 190.0F, 260.0F);
  const auto inspector_height = std::max(160.0F, extent.y - scene_height - gap);
  const auto lighting_height =
      std::max(140.0F, extent.y - renderer_height - performance_height - gap * 2.0F);
  const auto right_x = origin.x + extent.x - right_width;
  return {
      .scene = {{origin.x, origin.y}, {left_width, scene_height}},
      .inspector = {{origin.x, origin.y + scene_height + gap}, {left_width, inspector_height}},
      .lighting = {{right_x, origin.y + renderer_height + performance_height + gap * 2.0F},
                   {right_width, lighting_height}},
      .renderer = {{right_x, origin.y}, {right_width, renderer_height}},
      .performance = {{right_x, origin.y + renderer_height + gap},
                      {right_width, performance_height}},
  };
}

} // namespace

bool find_texture_preview(const gltf::texture_reference& reference, bool srgb,
                          std::span<const texture_preview> previews,
                          ImTextureID& texture) noexcept {
  texture = ImTextureID_Invalid;
  if (reference.image == gltf::invalid_index)
    return false;
  const auto found = std::ranges::find_if(previews, [&](const texture_preview& preview) {
    return preview.image == reference.image && preview.sampler == reference.sampler &&
           preview.srgb == srgb && preview.texture != ImTextureID_Invalid;
  });
  if (found == previews.end())
    return false;
  texture = found->texture;
  return true;
}

viewer_panel_changes draw_viewer_panels(const gltf::scene& scene, const viewer_state& state,
                                        const renderer_panel_info& renderer,
                                        const performance_panel_info& performance,
                                        std::span<const texture_preview> previews) {
  viewer_panel_changes changes;
  auto panels = state.panels();
  const auto layout = default_panel_layout();
  draw_panel("Scene", panels.scene, layout.scene,
             [&] { draw_scene_panel(scene, state, changes.state); });
  draw_panel("Inspector", panels.inspector, layout.inspector,
             [&] { draw_inspector_panel(scene, state, previews, changes); });
  draw_panel("Lighting", panels.lighting, layout.lighting,
             [&] { draw_lighting_panel(state, changes.state); });
  draw_panel("Renderer", panels.renderer, layout.renderer, [&] { draw_renderer_panel(renderer); });
  draw_panel("Performance", panels.performance, layout.performance,
             [&] { draw_performance_panel(performance); });
  if (panels.scene != state.panels().scene || panels.inspector != state.panels().inspector ||
      panels.lighting != state.panels().lighting || panels.renderer != state.panels().renderer ||
      panels.performance != state.panels().performance)
    changes.state.panels = panels;
  return changes;
}

} // namespace granit::example::model_viewer
