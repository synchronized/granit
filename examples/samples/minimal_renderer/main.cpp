// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <iostream>
#include <string_view>

namespace {

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " 失败: " << result.message() << "\n";
  return 1;
}

} // namespace

int main() {
  granit::renderer renderer;
  const granit::renderer_desc description{
      .application_name = "Granit Minimal Renderer",
      .enable_validation = false,
      .presentation = granit::presentation_mode::disabled,
  };

  auto result = renderer.initialize(description);
  if (result.failed())
    return report_failure("创建 Renderer", result);

  granit::renderer_info info;
  result = renderer.get_info(info);
  if (result.failed())
    return report_failure("查询 Renderer 信息", result);

  granit::renderer_limits limits;
  result = renderer.get_limits(limits);
  if (result.failed())
    return report_failure("查询 Renderer 限制", result);

  const auto version = granit::library_version();
  std::cout << "Granit " << version.major << '.' << version.minor << '.' << version.patch << "\n"
            << "Backend: " << static_cast<std::uint32_t>(info.backend) << "\n"
            << "Adapter: " << info.adapter_name << "\n"
            << "Max sampler anisotropy: " << limits.max_sampler_anisotropy << "\n";

  result = renderer.reset();
  if (result.failed())
    return report_failure("销毁 Renderer", result);
  return 0;
}
