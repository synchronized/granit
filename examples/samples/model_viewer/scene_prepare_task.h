// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_SCENE_PREPARE_TASK_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_SCENE_PREPARE_TASK_H_

#include "gltf/importer.h"

#include <granit/core/result.hpp>

#include <memory>

namespace granit::example::model_viewer {

class viewer_session;

/** 跨平台 CPU Scene 准备任务；平台实现决定是否使用工作线程。 */
class scene_prepare_task final {
public:
  scene_prepare_task();
  ~scene_prepare_task();
  scene_prepare_task(const scene_prepare_task&) = delete;
  scene_prepare_task& operator=(const scene_prepare_task&) = delete;

  [[nodiscard]] granit::result begin(viewer_session& session,
                                     gltf::import_progress_callback progress = nullptr,
                                     void* progress_user_data = nullptr) noexcept;
  /** 返回 not_ready 表示仍在执行，其他结果表示任务已经结束。 */
  [[nodiscard]] granit::result poll() noexcept;
  void reset() noexcept;
  [[nodiscard]] bool started() const noexcept { return static_cast<bool>(state_); }
  [[nodiscard]] bool running() const noexcept;

private:
  struct state;
  std::unique_ptr<state> state_;
};

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_SCENE_PREPARE_TASK_H_
