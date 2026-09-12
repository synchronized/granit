// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_OBJECT_BUILDER_HPP_
#define GRANIT_SHADER_OBJECT_BUILDER_HPP_

#include <granit/core/result.hpp>
#include <granit/tools/shader_object_builder.h>

#include <utility>

namespace granit::shader_tools {

inline std::pair<::granit::result, bool>
restore_object_cache(const granit_shader_tools_object_cache_desc& desc) noexcept {
  uint32_t cache_hit = 0;
  const auto status = granit_shader_tools_restore_object_cache(&desc, &cache_hit);
  return {::granit::from_native(status), cache_hit != 0};
}

} // namespace granit::shader_tools

#endif
