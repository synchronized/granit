// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_HANDLE_TABLE_H_
#define GRANIT_CORE_HANDLE_TABLE_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <granit/result.h>
#include <granit/types.h>

namespace granit::detail {

/** 内部资源类型。数值会编码进句柄，但不属于公共 ABI。 */
enum class resource_type : std::uint8_t {
  unknown = 0,
  renderer = 1,
  buffer = 2,
  texture = 3,
  shader = 4,
  pipeline = 5,
  swapchain = 6,
  fence = 7,
  surface = 8,
  texture_view = 9,
  sampler = 10,
};

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
  static constexpr std::uint32_t maximum_generation = UINT32_C(0x00ffffff);

  struct slot {
    void* resource{};
    std::uint32_t generation{1};
    std::uint32_t domain{};
    std::uint32_t next_free{invalid_slot};
    resource_type type{resource_type::unknown};
  };

  struct decoded_handle {
    std::uint32_t slot_index;
    std::uint32_t generation;
    resource_type type;
  };

  [[nodiscard]] static granit_handle encode(std::uint32_t slot_index, std::uint32_t generation,
                                            resource_type type) noexcept;
  [[nodiscard]] static bool decode(granit_handle handle, decoded_handle& decoded) noexcept;
  [[nodiscard]] const slot* validate(granit_handle handle, resource_type expected_type,
                                     std::uint32_t expected_domain,
                                     decoded_handle* decoded = nullptr) const noexcept;

  std::vector<slot> slots_;
  std::uint32_t free_head_{invalid_slot};
  std::size_t active_count_{};
};

} // namespace granit::detail

#endif
