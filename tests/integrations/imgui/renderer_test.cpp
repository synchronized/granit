// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/integrations/imgui/renderer.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace {

bool environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable || result == granit::result::not_ready;
}

struct resolver_context {
  std::vector<ImTextureID> textures;
  std::vector<granit_scissor> scissors;
};

granit::result resolve_texture(ImTextureID texture, granit_canvas_draw_state& state,
                               void* user_data) noexcept {
  auto& context = *static_cast<resolver_context*>(user_data);
  context.textures.push_back(texture);
  context.scissors.push_back(state.scissor);
  state.texture = static_cast<granit_texture_view>(texture + 100);
  state.sampler = 7;
  return granit::result::success;
}

void unsupported_callback(const ImDrawList*, const ImDrawCmd*) {}

struct draw_fixture {
  ImDrawList list{nullptr};
  ImDrawData data;

  draw_fixture() {
    list.VtxBuffer.push_back({{10.0F, 20.0F}, {0.0F, 0.0F}, IM_COL32_WHITE});
    list.VtxBuffer.push_back({{20.0F, 20.0F}, {1.0F, 0.0F}, IM_COL32_WHITE});
    list.VtxBuffer.push_back({{20.0F, 30.0F}, {1.0F, 1.0F}, IM_COL32_WHITE});
    list.VtxBuffer.push_back({{10.0F, 30.0F}, {0.0F, 1.0F}, IM_COL32_WHITE});
    for (const auto index : std::array<ImDrawIdx, 6>{0, 1, 2, 0, 1, 2})
      list.IdxBuffer.push_back(index);

    ImDrawCmd first;
    first.ClipRect = {5.0F, 15.0F, 40.0F, 50.0F};
    first.TexRef = ImTextureRef{11};
    first.ElemCount = 3;
    list.CmdBuffer.push_back(first);

    ImDrawCmd second;
    second.ClipRect = {30.0F, 30.0F, 100.0F, 100.0F};
    second.TexRef = ImTextureRef{22};
    second.VtxOffset = 1;
    second.IdxOffset = 3;
    second.ElemCount = 3;
    list.CmdBuffer.push_back(second);

    data.Valid = true;
    data.CmdLists.push_back(&list);
    data.DisplayPos = {10.0F, 20.0F};
    data.DisplaySize = {100.0F, 50.0F};
    data.FramebufferScale = {2.0F, 2.0F};
  }
};

} // namespace

TEST_CASE("ImGui Integration转换偏移、裁剪与多纹理Draw Data") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-imgui-integration"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::canvas_draw_list canvas;
  granit_canvas_draw_list_desc desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  REQUIRE(canvas.initialize(renderer.native_handle(), desc) == granit::result::success);

  draw_fixture fixture;
  resolver_context context;
  REQUIRE(granit::integration::imgui::append_draw_data(&fixture.data, canvas, resolve_texture,
                                                       &context) == granit::result::success);

  REQUIRE(context.textures == std::vector<ImTextureID>{11, 22});
  REQUIRE(context.scissors.size() == 2);
  CHECK(context.scissors[0].x == 0);
  CHECK(context.scissors[0].y == 0);
  CHECK(context.scissors[0].width == 60);
  CHECK(context.scissors[0].height == 60);
  CHECK(context.scissors[1].x == 40);
  CHECK(context.scissors[1].y == 20);
  CHECK(context.scissors[1].width == 140);
  CHECK(context.scissors[1].height == 80);

  granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  REQUIRE(canvas.get_stats(stats) == granit::result::success);
  CHECK(stats.vertex_count == 4);
  CHECK(stats.index_count == 6);
  CHECK(stats.item_count == 2);
  CHECK(stats.batch_count == 2);
}

TEST_CASE("ImGui Integration处理空数据并拒绝不支持的回调") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-imgui-errors"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::canvas_draw_list canvas;
  granit_canvas_draw_list_desc desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  REQUIRE(canvas.initialize(renderer.native_handle(), desc) == granit::result::success);

  ImDrawData empty;
  empty.Valid = true;
  resolver_context context;
  CHECK(granit::integration::imgui::append_draw_data(&empty, canvas, resolve_texture, &context) ==
        granit::result::success);
  CHECK(context.textures.empty());

  draw_fixture fixture;
  fixture.list.CmdBuffer[0].UserCallback = unsupported_callback;
  CHECK(granit::integration::imgui::append_draw_data(&fixture.data, canvas, resolve_texture,
                                                     &context) == granit::result::unsupported);
  CHECK(context.textures.empty());

  fixture.list.CmdBuffer[0].UserCallback = nullptr;
  fixture.list.CmdBuffer[0].IdxOffset = 7;
  CHECK(granit::integration::imgui::append_draw_data(&fixture.data, canvas, resolve_texture,
                                                     &context) == granit::result::invalid_argument);
}
