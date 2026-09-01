// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_panels.h"

#include <catch2/catch_all.hpp>
#include <imgui.h>

#include <array>

namespace {

struct imgui_context {
  imgui_context() { ImGui::CreateContext(); }
  ~imgui_context() { ImGui::DestroyContext(); }
};

} // namespace

TEST_CASE("材质缩略图按 Image 与颜色空间查找", "[example][model-viewer][imgui][texture]") {
  using namespace granit::example;
  const std::array previews{
      model_viewer::texture_preview{.image = 2, .sampler = 4, .srgb = true, .texture = 11},
      model_viewer::texture_preview{.image = 2, .sampler = 4, .srgb = false, .texture = 22}};
  const gltf::texture_reference reference{.image = 2, .sampler = 4};
  ImTextureID texture = ImTextureID_Invalid;
  REQUIRE(model_viewer::find_texture_preview(reference, true, previews, texture));
  CHECK(texture == 11);
  REQUIRE(model_viewer::find_texture_preview(reference, false, previews, texture));
  CHECK(texture == 22);
  const gltf::texture_reference missing{.image = 3};
  CHECK_FALSE(model_viewer::find_texture_preview(missing, true, previews, texture));
  CHECK(texture == ImTextureID_Invalid);
}

TEST_CASE("查看器 ImGui 面板可在无平台后端上下文中构建", "[example][model-viewer][imgui]") {
  imgui_context context;
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = {1280, 720};
  io.DeltaTime = 1.0F / 60.0F;
  unsigned char* pixels{};
  int texture_width{};
  int texture_height{};
  io.Fonts->GetTexDataAsRGBA32(&pixels, &texture_width, &texture_height);
  io.Fonts->SetTexID(1);
  io.Fonts->TexRef._TexData->SetStatus(ImTextureStatus_OK);

  granit::example::gltf::scene scene;
  scene.nodes.emplace_back();
  scene.nodes.back().name = "Helmet";
  scene.materials.emplace_back();
  scene.materials.back().name = "Metal";
  granit::example::model_viewer::viewer_state state;
  state.reset(scene);

  const granit::example::model_viewer::renderer_panel_info renderer{.backend = "Vulkan",
                                                                    .adapter = "Test Adapter",
                                                                    .swapchain_format = "BGRA8",
                                                                    .present_mode = "FIFO",
                                                                    .width = 1280,
                                                                    .height = 720,
                                                                    .frame_slots = 3};
  const granit::example::model_viewer::performance_panel_info performance{
      .frames_per_second = 60.0F, .cpu_frame_ms = 2.0F};
  ImGui::NewFrame();
  static_cast<void>(
      granit::example::model_viewer::draw_viewer_panels(scene, state, renderer, performance));
  ImGui::Render();

  ImGui::NewFrame();
  const auto change =
      granit::example::model_viewer::draw_viewer_panels(scene, state, renderer, performance);
  ImGui::Render();
  REQUIRE(ImGui::GetDrawData() != nullptr);
  CHECK(ImGui::GetDrawData()->Valid);
  CHECK_FALSE(change.state.selected_node.has_value());
  CHECK_FALSE(change.material.has_value());
}
