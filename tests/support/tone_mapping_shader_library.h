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
  [[nodiscard]] bool initialize(granit_renderer renderer) {
    const auto vertex_path =
        std::string{GRANIT_PIPELINE_SHADER_DIR} + "/tone_mapping.vert.grshader";
    const auto fragment_path =
        std::string{GRANIT_PIPELINE_SHADER_DIR} + "/tone_mapping.frag.grshader";
    if (!assets_.add(vertex_path) || !assets_.add(fragment_path) ||
        !assets_.initialize_library(renderer, bytes_, library_)) {
      return false;
    }
    vertex_id_ = assets_.reference(vertex_path).asset_id;
    fragment_id_ = assets_.reference(fragment_path).asset_id;
    return true;
  }

  [[nodiscard]] const granit::shader_library& library() const noexcept { return library_; }
  [[nodiscard]] const granit::shader_content_id& vertex_id() const noexcept { return vertex_id_; }
  [[nodiscard]] const granit::shader_content_id& fragment_id() const noexcept {
    return fragment_id_;
  }

private:
  shader_asset_store assets_;
  std::vector<std::byte> bytes_;
  granit::shader_library library_;
  granit::shader_content_id vertex_id_{};
  granit::shader_content_id fragment_id_{};
};

} // namespace granit::tests

#endif
