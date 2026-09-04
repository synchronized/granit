// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_CALLBACK_LIFETIME_H_
#define GRANIT_BACKEND_CALLBACK_LIFETIME_H_

#include <memory>
#include <mutex>
#include <utility>

namespace granit::detail {

class backend_callback_ticket;

/** 将异步回调与 Provider 所有者的销毁顺序同步。 */
class backend_callback_lifetime {
public:
  backend_callback_lifetime();
  ~backend_callback_lifetime() { invalidate(); }

  backend_callback_lifetime(const backend_callback_lifetime&) = delete;
  backend_callback_lifetime& operator=(const backend_callback_lifetime&) = delete;

  [[nodiscard]] backend_callback_ticket ticket() const noexcept;
  /** 返回前保证没有回调仍在执行，且后续票据调用不会进入回调。 */
  void invalidate() noexcept;

private:
  struct control {
    // WebGPU AllowSpontaneous 回调可能在发起下一级请求时重入同一线程。
    std::recursive_mutex mutex;
    bool active{true};
  };

  std::shared_ptr<control> control_;
  friend class backend_callback_ticket;
};

class backend_callback_ticket {
public:
  backend_callback_ticket() = default;

  template <typename Callback> [[nodiscard]] bool invoke(Callback&& callback) const {
    const auto control = control_.lock();
    if (!control)
      return false;
    std::lock_guard lock{control->mutex};
    if (!control->active)
      return false;
    std::forward<Callback>(callback)();
    return true;
  }

private:
  explicit backend_callback_ticket(const std::shared_ptr<backend_callback_lifetime::control>& state)
      : control_(state) {}

  std::weak_ptr<backend_callback_lifetime::control> control_;
  friend class backend_callback_lifetime;
};

inline backend_callback_lifetime::backend_callback_lifetime()
    : control_(std::make_shared<control>()) {}

inline backend_callback_ticket backend_callback_lifetime::ticket() const noexcept {
  return backend_callback_ticket{control_};
}

inline void backend_callback_lifetime::invalidate() noexcept {
  const auto control = std::exchange(control_, {});
  if (!control)
    return;
  std::lock_guard lock{control->mutex};
  control->active = false;
}

} // namespace granit::detail

#endif
