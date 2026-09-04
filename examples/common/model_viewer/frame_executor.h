// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_FRAME_EXECUTOR_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_FRAME_EXECUTOR_H_

#include "model_viewer/application_core.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace granit::example::model_viewer {

/** 单帧执行完成后返回给平台主循环的数据。 */
struct frame_execution_result {
  bool needs_recreate{};
  float acquire_wait_ms{};
  float present_wait_ms{};
};

/** 异步帧执行完成回执。dropped 表示帧在开始执行前被更新帧替换。 */
struct frame_completion {
  std::uint64_t sequence{};
  granit::result status{granit::result::unknown};
  frame_execution_result execution;
  bool dropped{};
};

using frame_execute_callback = granit::result (*)(frame_packet&& packet,
                                                  frame_execution_result& output, void* user_data);

/** 示例私有帧执行边界；实现负责完整消费传入的不可变帧包。 */
class frame_executor {
public:
  virtual ~frame_executor() = default;

  [[nodiscard]] virtual granit::result submit(frame_packet packet,
                                              frame_execution_result& output) = 0;
  [[nodiscard]] virtual granit::result flush() noexcept = 0;
};

/** 在调用线程立即执行帧的实现，供同步平台和线程迁移前的桌面路径使用。 */
class inline_frame_executor final : public frame_executor {
public:
  inline_frame_executor(frame_execute_callback callback, void* user_data) noexcept;

  [[nodiscard]] granit::result submit(frame_packet packet, frame_execution_result& output) override;
  [[nodiscard]] granit::result flush() noexcept override;

private:
  frame_execute_callback callback_{};
  void* user_data_{};
};

/** 桌面用有界异步执行器；所有回调只在其专用工作线程串行执行。 */
class threaded_frame_executor final {
public:
  threaded_frame_executor();
  ~threaded_frame_executor();
  threaded_frame_executor(const threaded_frame_executor&) = delete;
  threaded_frame_executor& operator=(const threaded_frame_executor&) = delete;

  [[nodiscard]] granit::result initialize(frame_execute_callback callback, void* user_data,
                                          std::size_t maximum_pending_frames = 3) noexcept;
  [[nodiscard]] granit::result submit(frame_packet packet, std::uint64_t& sequence) noexcept;
  [[nodiscard]] bool try_take_completion(frame_completion& completion) noexcept;
  [[nodiscard]] granit::result flush() noexcept;
  void stop() noexcept;
  [[nodiscard]] bool running() const noexcept;

private:
  struct state;
  std::unique_ptr<state> state_;
};

} // namespace granit::example::model_viewer

#endif
