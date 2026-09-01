// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_panels.h"

#include <imgui.h>

#include <array>
#include <vector>

namespace granit::example::model_viewer {
namespace {

const char* node_name(const gltf::node& node) noexcept {
  return node.name.empty() ? "Unnamed Node" : node.name.c_str();
}

const char* material_name(const gltf::material& material) noexcept {
  return material.name.empty() ? "Unnamed Material" : material.name.c_str();
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
  }
}

void draw_lighting_panel(const viewer_state& state, viewer_change& change) {
  auto light = state.directional_light();
  auto exposure = state.exposure_ev();
  bool changed = ImGui::DragFloat3("Direction", &light.direction.x, 0.01F, -1.0F, 1.0F);
  changed |= ImGui::DragFloat3("Radiance", &light.radiance.x, 0.05F, 0.0F, 100000.0F);
  if (changed)
    change.directional_light = light;
  if (ImGui::SliderFloat("Exposure EV", &exposure, -24.0F, 24.0F))
    change.exposure_ev = exposure;

  static constexpr std::array modes{"Shaded", "Base Color", "Normals", "Metallic", "Roughness"};
  auto mode = static_cast<int>(state.debug_display());
  if (ImGui::Combo("Debug Display", &mode, modes.data(), static_cast<int>(modes.size())))
    change.debug_display = static_cast<debug_display_mode>(mode);
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
}

template <typename Callback> void draw_panel(const char* name, bool& open, Callback&& callback) {
  if (!open)
    return;
  if (ImGui::Begin(name, &open))
    callback();
  ImGui::End();
}

} // namespace

viewer_panel_changes draw_viewer_panels(const gltf::scene& scene, const viewer_state& state,
                                        const renderer_panel_info& renderer,
                                        const performance_panel_info& performance) {
  viewer_panel_changes changes;
  auto panels = state.panels();
  draw_panel("Scene", panels.scene, [&] { draw_scene_panel(scene, state, changes.state); });
  draw_panel("Inspector", panels.inspector, [&] { draw_inspector_panel(scene, state, changes); });
  draw_panel("Lighting", panels.lighting, [&] { draw_lighting_panel(state, changes.state); });
  draw_panel("Renderer", panels.renderer, [&] { draw_renderer_panel(renderer); });
  draw_panel("Performance", panels.performance, [&] { draw_performance_panel(performance); });
  if (panels.scene != state.panels().scene || panels.inspector != state.panels().inspector ||
      panels.lighting != state.panels().lighting || panels.renderer != state.panels().renderer ||
      panels.performance != state.panels().performance)
    changes.state.panels = panels;
  return changes;
}

} // namespace granit::example::model_viewer
