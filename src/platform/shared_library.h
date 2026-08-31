// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PLATFORM_SHARED_LIBRARY_H_
#define GRANIT_PLATFORM_SHARED_LIBRARY_H_

#include <string>

namespace granit::detail::platform {

/** 返回当前 Granit Core 模块所在目录；失败时返回空字符串。 */
[[nodiscard]] std::string module_directory() noexcept;

/** 拥有一个平台动态库句柄；只接受显式绝对路径。 */
class shared_library {
public:
  shared_library() = default;
  ~shared_library();

  shared_library(const shared_library&) = delete;
  shared_library& operator=(const shared_library&) = delete;
  shared_library(shared_library&& other) noexcept;
  shared_library& operator=(shared_library&& other) noexcept;

  [[nodiscard]] bool open(const char* absolute_path) noexcept;
  void close() noexcept;

  [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }
  [[nodiscard]] void* symbol(const char* name) const noexcept;

private:
  void* handle_{nullptr};
};

} // namespace granit::detail::platform

#endif
