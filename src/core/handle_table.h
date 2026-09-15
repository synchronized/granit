// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_HANDLE_TABLE_H_
#define GRANIT_CORE_HANDLE_TABLE_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <granit/core/result.h>
#include <granit/core/types.h>

#include "core/handle_encoding.h"

namespace granit::detail {

using resource_type = handle_type;

/**
 * 非拥有资源句柄表。
 *
 * 表只记录资源地址和校验元数据，不负责释放资源。调用者必须在资源销毁前擦除对应句柄，
 * 并在外部保证访问与销毁不会并发发生。
 */
class handle_table {
public:
  handle_table() = default;

  handle_table(const handle_table&) = delete;
  handle_table& operator=(const handle_table&) = delete;
  handle_table(handle_table&&) = delete;
  handle_table& operator=(handle_table&&) = delete;

  /** 注册资源。失败时返回空句柄。 */
  [[nodiscard]] granit_handle insert(void* resource, resource_type type, std::uint32_t domain);

  /** 查找并验证资源；任何校验失败均返回空指针。 */
  [[nodiscard]] void* find(granit_handle handle, resource_type expected_type,
                           std::uint32_t expected_domain) const noexcept;

  /** 擦除资源句柄，可选择取回此前注册的非拥有地址。 */
  [[nodiscard]] granit_result erase(granit_handle handle, resource_type expected_type,
                                    std::uint32_t expected_domain,
                                    void** resource = nullptr) noexcept;

  [[nodiscard]] std::size_t size() const noexcept { return active_count_; }
  [[nodiscard]] bool empty() const noexcept { return active_count_ == 0; }

private:
  static constexpr std::uint32_t invalid_slot = std::numeric_limits<std::uint32_t>::max();
  struct slot {
    void* resource{};
    std::uint32_t generation{1};
    std::uint32_t domain{};
    std::uint32_t next_free{invalid_slot};
    resource_type type{resource_type::unknown};
  };

  [[nodiscard]] const slot* validate(granit_handle handle, resource_type expected_type,
                                     std::uint32_t expected_domain,
                                     decoded_handle* decoded = nullptr) const noexcept;

  std::vector<slot> slots_;
  std::uint32_t free_head_{invalid_slot};
  std::size_t active_count_{};
};

} // namespace granit::detail

#endif
