// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <iostream>
#include <span>

int main() {
  granit::renderer renderer;
  auto result = renderer.initialize(
      {.application_name = "Granit Offscreen Clear", .enable_validation = true});
  if (result.failed()) {
    std::cerr << "创建 Renderer 失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.width = 256;
  texture_desc.height = 256;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  result = granit::from_native(granit_texture_create_with_default_view(
      renderer.native_handle(), &texture_desc, &texture, &view));
  if (result.failed()) {
    std::cerr << "创建离屏纹理失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  granit::command_recorder recorder;
  result = recorder.initialize(renderer.native_handle());
  if (result.ok())
    result = recorder.begin();
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.08F, .green = 0.16F, .blue = 0.3F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, 256, 256}};
  if (result.ok())
    result = recorder.begin_rendering(rendering);
  if (result.ok())
    result = recorder.end_rendering();
  if (result.ok())
    result = recorder.end();
  if (result.ok())
    result = recorder.submit();
  if (result.ok())
    result = recorder.reset();
  static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), view));
  static_cast<void>(granit_texture_destroy(renderer.native_handle(), texture));
  if (result.failed()) {
    std::cerr << "离屏清屏失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  std::cout << "离屏清屏完成\n";
  return 0;
}
