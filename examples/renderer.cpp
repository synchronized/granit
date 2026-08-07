// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer.hpp>

#include <iostream>

int main() {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "Granit Renderer Example"});
  if (granit::failed(result)) {
    std::cerr << "创建 renderer 失败: " << granit::result_message(result) << '\n';
    return 1;
  }

  std::cout << "renderer 创建成功\n";
  return 0;
}
