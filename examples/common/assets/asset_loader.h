// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOADER_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOADER_H_

#include "assets/asset_request.h"

#include <memory>
#include <string>

namespace granit::example::assets {

/** 异步读取资产字节；构建时选择 Desktop 或 Web 后端。 */
class asset_loader {
public:
  asset_loader();
  ~asset_loader();

  asset_loader(const asset_loader&) = delete;
  asset_loader& operator=(const asset_loader&) = delete;

  /** 启动读取；location 在 Desktop 是文件路径，在 Web 是 URL。 */
  [[nodiscard]] std::shared_ptr<asset_request> load(std::string location);

  /** 在调用线程发布已经完成的后端结果。 */
  void poll();

private:
  struct implementation;
  std::unique_ptr<implementation> implementation_;
};

} // namespace granit::example::assets

#endif
