// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.hpp>

#include <iostream>

int main() {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "Granit Renderer Example"});
  if (!result) {
    std::cerr << "创建 renderer 失败: " << result.message() << '\n';
    return 1;
  }

  std::cout << "renderer 创建成功\n";
  return 0;
}
