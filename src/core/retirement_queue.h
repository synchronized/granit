// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_RETIREMENT_QUEUE_H_
#define GRANIT_CORE_RETIREMENT_QUEUE_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace granit::detail {

using submission_serial = std::uint64_t;

/** 管理单个 Renderer 的单调提交序号。调用者负责外部同步。 */
class submission_serials {
public:
  [[nodiscard]] submission_serial next() const noexcept;
  [[nodiscard]] bool commit(submission_serial serial) noexcept;
  void mark_completed(submission_serial serial) noexcept;

  [[nodiscard]] submission_serial last_submitted() const noexcept { return last_submitted_; }
  [[nodiscard]] submission_serial completed() const noexcept { return completed_; }

private:
  submission_serial last_submitted_{};
  submission_serial completed_{};
};

/** 同一完成点内，引用对象必须在被引用的基础资源之前释放。 */
enum class retirement_order : std::uint8_t {
  dependent,
  resource,
};

/**
 * 不含后端类型的资源退役队列。
 *
 * 队列持有资源的最后一个内部强引用；条目被收集时触发实际析构。调用者必须保证资源析构
 * 不抛异常，并且不在持有 Registry 锁时调用收集操作。
 */
class retirement_queue {
public:
  void retire(submission_serial retire_after, retirement_order order,
              std::shared_ptr<void> resource);

  /** 释放完成序号以内的所有批次，返回释放的条目数。 */
  std::size_t collect(submission_serial completed_serial) noexcept;

  /** Renderer 已确认 GPU 空闲后，无条件释放全部条目。 */
  std::size_t drain() noexcept;

  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] submission_serial earliest_serial() const noexcept;

private:
  struct retirement_batch {
    std::vector<std::shared_ptr<void>> dependents;
    std::vector<std::shared_ptr<void>> resources;

    [[nodiscard]] std::size_t size() const noexcept { return dependents.size() + resources.size(); }
  };

  std::map<submission_serial, retirement_batch> batches_;
  std::size_t size_{};
};

} // namespace granit::detail

#endif
