// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_SWAPCHAIN_FRAME_H_
#define GRANIT_TESTS_SUPPORT_SWAPCHAIN_FRAME_H_

#include <granit/granit.hpp>

#include <cstdint>
#include <span>

namespace granit::tests {

/** 执行一帧最小 Swapchain 清屏、提交与呈现，供不同窗口入口共享。 */
inline result render_clear_frame(swapchain& chain, frame_context& context, std::uint32_t width,
                                 std::uint32_t height, bool& needs_recreate) {
  acquired_frame frame;
  auto status = chain.acquire(frame);
  if (status.failed())
    return status;
  needs_recreate = frame.needs_recreate;

  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  status = chain.backbuffer(frame.image_index, texture, view);
  frame_recording recording;
  if (status.ok())
    status = context.begin(frame, recording);
  const color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.04F, .green = 0.12F, .blue = 0.22F, .alpha = 1.0F}};
  const rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                 .area = {0, 0, width, height}};
  if (status.ok())
    status = recording.recorder().begin_rendering(rendering);
  if (status.ok())
    status = recording.recorder().end_rendering();
  if (status.ok())
    status = recording.submit();
  if (status.ok())
    status = chain.present(frame);
  needs_recreate = needs_recreate || frame.needs_recreate;
  if (status.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(chain.cancel(frame));
  }
  return status;
}

inline result render_clear_frames(swapchain& chain, frame_context& context, std::uint32_t width,
                                  std::uint32_t height, std::uint32_t frame_count) {
  std::uint32_t rendered_frames = 0;
  std::uint32_t attempts = 0;
  while (rendered_frames < frame_count && attempts++ < frame_count * 3U) {
    bool needs_recreate = false;
    auto status = render_clear_frame(chain, context, width, height, needs_recreate);
    if (status == result::out_of_date) {
      status = chain.recreate({.width = width, .height = height});
      if (status.failed())
        return status;
      continue;
    }
    if (status.failed())
      return status;
    ++rendered_frames;
    if (needs_recreate) {
      status = chain.recreate({.width = width, .height = height});
      if (status.failed())
        return status;
    }
  }
  return rendered_frames == frame_count ? result::success : result::not_ready;
}

} // namespace granit::tests

#endif
