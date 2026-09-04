// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DRAW_BINDING_CACHE_H_
#define GRANIT_PIPELINE_DRAW_BINDING_CACHE_H_

#include <granit/core/result.h>

#include <cstddef>

namespace granit::pipeline::detail {

/** Dynamic Uniform Arena 启用后释放旧的逐 Draw Uniform 绑定。 */
template <typename Entries> granit_result release_legacy_uniform_bindings(Entries& entries) {
  auto result = GRANIT_SUCCESS;
  for (auto& entry : entries) {
    const auto reset_result = entry.bindings.reset();
    if (result == GRANIT_SUCCESS)
      result = reset_result;
  }
  return result;
}

/** 回收不再对应当前 Draw 的尾部绑定缓存。 */
template <typename Entries>
granit_result trim_draw_binding_cache(Entries& entries, std::size_t retained_count) {
  auto result = GRANIT_SUCCESS;
  while (entries.size() > retained_count) {
    const auto binding_result = entries.back().bindings.reset();
    if (result == GRANIT_SUCCESS)
      result = binding_result;
    const auto lighting_result = entries.back().lighting.reset();
    if (result == GRANIT_SUCCESS)
      result = lighting_result;
    entries.pop_back();
  }
  return result;
}

} // namespace granit::pipeline::detail

#endif
