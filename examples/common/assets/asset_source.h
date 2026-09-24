// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SOURCE_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SOURCE_H_

#include "assets/asset_request.h"

#include <memory>
#include <string>

namespace granit::example::assets {

/** Asset System 私有的平台读取源；构建时选择 Desktop 或 Web 实现。 */
class asset_source {
public:
  asset_source();
  ~asset_source();

  asset_source(const asset_source&) = delete;
  asset_source& operator=(const asset_source&) = delete;

  /** 启动读取；location 在 Desktop 是文件路径，在 Web 是 URL。 */
  [[nodiscard]] std::shared_ptr<asset_request> load(std::string location);

  /** 在调用线程发布已经完成的后端结果。 */
  void poll();

private:
  struct implementation;
  std::unique_ptr<implementation> implementation_;
};

} // namespace granit::example::assets

#endif // GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SOURCE_H_
