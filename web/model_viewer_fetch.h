// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WEB_MODEL_VIEWER_FETCH_H_
#define GRANIT_WEB_MODEL_VIEWER_FETCH_H_

#include "asset_request.h"

#include <memory>
#include <string_view>

namespace granit::example::model_viewer::web {

/** 使用 Emscripten Fetch 填充共享资产请求状态。 */
[[nodiscard]] bool start_fetch(const std::shared_ptr<asset_request>& request, std::string_view url);

} // namespace granit::example::model_viewer::web

#endif
