// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_TONE_MAPPING_SHADER_LIBRARY_H_
#define GRANIT_TESTS_SUPPORT_TONE_MAPPING_SHADER_LIBRARY_H_

#include "shader_asset_store.h"

#include <cstddef>
#include <string>
#include <vector>

namespace granit::tests {

/** 从测试构建目录的中间资产组装 Tone Mapping Library，并持有其借用字节。 */
class tone_mapping_shader_library {
public:
  [[nodiscard]] bool initialize(granit::renderer& renderer) {
    const auto vertex_path =
        std::string{GRANIT_PIPELINE_SHADER_DIR} + "/tone_mapping.vert.grshaderobj";
    const auto fragment_path =
        std::string{GRANIT_PIPELINE_SHADER_DIR} + "/tone_mapping.frag.grshaderobj";
    if (!assets_.add(vertex_path, vertex_name()) || !assets_.add(fragment_path, fragment_name()) ||
        !assets_.initialize_library(renderer, bytes_, library_)) {
      return false;
    }
    return true;
  }

  [[nodiscard]] const granit::shader_library& library() const noexcept { return library_; }
  [[nodiscard]] static constexpr std::string_view vertex_name() noexcept {
    return "tone_mapping.vertex";
  }
  [[nodiscard]] static constexpr std::string_view fragment_name() noexcept {
    return "tone_mapping.fragment";
  }

private:
  shader_asset_store assets_;
  std::vector<std::byte> bytes_;
  granit::shader_library library_;
};

} // namespace granit::tests

#endif
