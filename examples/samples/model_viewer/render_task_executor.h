// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_RENDER_TASK_EXECUTOR_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_RENDER_TASK_EXECUTOR_H_

#include "imgui/frame_canvas_data.h"
#include "model_viewer/application_core.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace granit::example::model_viewer {

/** 执行层拥有的完整帧包；在 Core 渲染数据之外携带可选 UI Canvas。 */
struct frame_packet {
  viewer_frame viewer;
  imgui::frame_canvas_data canvas;
};

/** 单帧执行完成后返回给平台主循环的数据。 */
struct frame_execution_result {
  bool needs_recreate{};
  float queue_wait_ms{};
  float acquire_wait_ms{};
  float present_wait_ms{};
  float gpu_frame_ms{};
  bool gpu_timing_available{};
};

/** 异步帧执行完成回执。dropped 表示帧在开始执行前被更新帧替换。 */
struct frame_completion {
  std::uint64_t sequence{};
  granit::result status{granit::result::unknown};
  frame_execution_result execution;
  bool dropped{};
};

using render_frame_callback =
    std::function<granit::result(frame_packet&& packet, frame_execution_result& output)>;
using render_control_task = std::function<granit::result()>;

/** 不可丢弃命令的完成回执。 */
struct render_task_completion {
  std::uint64_t sequence{};
  granit::result status{granit::result::unknown};
};

struct render_task_queue_stats {
  std::size_t pending_high_watermark{};
  std::uint64_t replaced_frames{};
  std::uint64_t skipped_frame_builds{};
  /** 被替换帧在队列中滞留的累计时长，直接度量渲染跟不上输入的滞后。 */
  float render_lag_ms{};
};

/** 示例私有帧执行边界；实现负责完整消费传入的不可变帧包。 */
class render_task_executor {
public:
  virtual ~render_task_executor() = default;

  [[nodiscard]] virtual granit::result initialize(render_frame_callback callback) noexcept = 0;
  [[nodiscard]] virtual granit::result submit(frame_packet packet,
                                              frame_execution_result& output) = 0;
  [[nodiscard]] virtual granit::result run_task(render_control_task task) noexcept = 0;
  [[nodiscard]] virtual granit::result flush() noexcept = 0;
  virtual void stop() noexcept = 0;
  [[nodiscard]] virtual bool running() const noexcept = 0;
};

/** 在调用线程立即执行帧的实现，供同步平台和线程迁移前的桌面路径使用。 */
class inline_render_task_executor final : public render_task_executor {
public:
  inline_render_task_executor() = default;
  explicit inline_render_task_executor(render_frame_callback callback) noexcept;

  [[nodiscard]] granit::result initialize(render_frame_callback callback) noexcept override;
  [[nodiscard]] granit::result submit(frame_packet packet, frame_execution_result& output) override;
  [[nodiscard]] granit::result run_task(render_control_task task) noexcept override;
  template <typename Task>
    requires(!std::is_same_v<std::remove_cvref_t<Task>, render_control_task>)
  [[nodiscard]] granit::result run_task(Task&& task) noexcept {
    try {
      return run_task(render_control_task{std::forward<Task>(task)});
    } catch (const std::bad_alloc&) {
      return granit::result::out_of_memory;
    } catch (...) {
      return granit::result::internal;
    }
  }
  [[nodiscard]] granit::result flush() noexcept override;
  void stop() noexcept override;
  [[nodiscard]] bool running() const noexcept override;

private:
  render_frame_callback callback_;
};

/** 桌面用有界异步执行器；所有回调只在其专用工作线程串行执行。 */
class threaded_render_task_executor final : public render_task_executor {
public:
  threaded_render_task_executor();
  ~threaded_render_task_executor();
  threaded_render_task_executor(const threaded_render_task_executor&) = delete;
  threaded_render_task_executor& operator=(const threaded_render_task_executor&) = delete;

  [[nodiscard]] granit::result initialize(render_frame_callback callback,
                                          std::size_t maximum_pending_frames) noexcept;
  [[nodiscard]] granit::result initialize(render_frame_callback callback) noexcept override;
  [[nodiscard]] granit::result submit(frame_packet packet, frame_execution_result& output) override;
  [[nodiscard]] granit::result submit(frame_packet packet, std::uint64_t& sequence) noexcept;
  /** 提交拥有其捕获数据的控制任务；队列已满时返回 not_ready，不替换已有任务。 */
  [[nodiscard]] granit::result submit_task(render_control_task task,
                                           std::uint64_t& sequence) noexcept;
  template <typename Task>
    requires(!std::is_same_v<std::remove_cvref_t<Task>, render_control_task>)
  [[nodiscard]] granit::result submit_task(Task&& task, std::uint64_t& sequence) noexcept {
    try {
      return submit_task(render_control_task{std::forward<Task>(task)}, sequence);
    } catch (const std::bad_alloc&) {
      return granit::result::out_of_memory;
    } catch (...) {
      return granit::result::internal;
    }
  }
  /** 提交不可丢弃控制任务并等待其完成。 */
  [[nodiscard]] granit::result run_task(render_control_task task) noexcept override;
  template <typename Task>
    requires(!std::is_same_v<std::remove_cvref_t<Task>, render_control_task>)
  [[nodiscard]] granit::result run_task(Task&& task) noexcept {
    try {
      return run_task(render_control_task{std::forward<Task>(task)});
    } catch (const std::bad_alloc&) {
      return granit::result::out_of_memory;
    } catch (...) {
      return granit::result::internal;
    }
  }
  /** 返回当前是否有待处理帧容量；单生产者仍须处理 submit 的最终结果。 */
  [[nodiscard]] bool can_submit_frame() const noexcept;
  /** 记录调用方因容量不足而在构造前跳过的帧。 */
  void record_skipped_frame_build() noexcept;
  [[nodiscard]] bool try_take_completion(frame_completion& completion) noexcept;
  [[nodiscard]] bool try_take_task_completion(render_task_completion& completion) noexcept;
  [[nodiscard]] render_task_queue_stats query_queue_stats() const noexcept;
  [[nodiscard]] granit::result flush() noexcept override;
  void stop() noexcept override;
  [[nodiscard]] bool running() const noexcept override;

private:
  struct state;
  std::unique_ptr<state> state_;
};

} // namespace granit::example::model_viewer

#endif
