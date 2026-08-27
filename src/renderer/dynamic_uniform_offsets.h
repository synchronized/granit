// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_DYNAMIC_UNIFORM_OFFSETS_H_
#define GRANIT_RENDERER_DYNAMIC_UNIFORM_OFFSETS_H_

#include <algorithm>
#include <cstdint>
#include <span>

namespace granit::detail {

/** 单个动态 Uniform Binding 的不可变范围信息，按公开 Offset 消费顺序保存。 */
struct dynamic_uniform_binding {
  std::uint32_t binding{};
  std::uint64_t base_offset{};
  std::uint64_t range{};
  std::uint64_t buffer_size{};
};

inline void sort_dynamic_uniform_bindings(std::span<dynamic_uniform_binding> bindings) noexcept {
  std::sort(bindings.begin(), bindings.end(),
            [](const auto& left, const auto& right) { return left.binding < right.binding; });
}

/** 校验扁平动态 Offset 数组；调用方负责按 Bind Group 和 binding 顺序拼接元数据。 */
[[nodiscard]] inline bool
validate_dynamic_uniform_offsets(std::span<const dynamic_uniform_binding> bindings,
                                 std::span<const std::uint32_t> offsets,
                                 std::uint64_t alignment) noexcept {
  if (bindings.size() != offsets.size())
    return false;
  for (std::size_t index = 0; index < bindings.size(); ++index) {
    const auto& binding = bindings[index];
    const auto dynamic_offset = static_cast<std::uint64_t>(offsets[index]);
    if ((alignment != 0 && dynamic_offset % alignment != 0) ||
        binding.base_offset > binding.buffer_size ||
        dynamic_offset > binding.buffer_size - binding.base_offset ||
        binding.range > binding.buffer_size - binding.base_offset - dynamic_offset)
      return false;
  }
  return true;
}

} // namespace granit::detail

#endif
