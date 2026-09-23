// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_STORE_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_STORE_H_

#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

namespace granit::example::assets {

/** 示例私有的只读资产入口；路径相对于运行时 assets 目录。 */
class asset_store {
public:
  /** 根据可执行文件路径确定运行时 assets 目录。 */
  [[nodiscard]] bool initialize(std::string_view executable_path);

  /** 读取逻辑路径对应的完整文件；拒绝绝对路径和父目录跳转。 */
  [[nodiscard]] bool read(std::string_view logical_path, std::vector<std::byte>& output) const;

  [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

private:
  std::filesystem::path root_;
};

} // namespace granit::example::assets

#endif
