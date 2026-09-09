// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_WEB_RESOURCE_FETCH_BATCH_H_
#define GRANIT_EXAMPLES_COMMON_WEB_RESOURCE_FETCH_BATCH_H_

#include "web/asset_request.h"
#include "web/resource_bundle.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace granit::example::web {

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

} // namespace granit::example::web

#endif
