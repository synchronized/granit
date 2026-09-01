// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PLATFORM_WEB_RESOURCE_FETCH_BATCH_H_
#define GRANIT_EXAMPLES_PLATFORM_WEB_RESOURCE_FETCH_BATCH_H_

#include "asset_request.h"
#include "resource_bundle.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace granit::example::model_viewer::web {

enum class resource_fetch_batch_status { idle, pending, ready, failed };

struct resource_fetch_entry {
  std::string path;
  std::string url;
  std::shared_ptr<asset_request> request;
};

/** 汇总一组浏览器资源请求，并在全部完成后原子填充资源包。 */
class resource_fetch_batch {
public:
  [[nodiscard]] bool add(std::string_view path, std::string url);
  [[nodiscard]] resource_fetch_batch_status status() const noexcept;
  [[nodiscard]] bool commit(resource_bundle& bundle) const;
  void clear() noexcept;

  [[nodiscard]] const std::vector<resource_fetch_entry>& entries() const noexcept {
    return entries_;
  }

private:
  std::vector<resource_fetch_entry> entries_;
};

} // namespace granit::example::model_viewer::web

#endif
