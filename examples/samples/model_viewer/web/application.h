// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLE_MODEL_VIEWER_WEB_APPLICATION_H_
#define GRANIT_EXAMPLE_MODEL_VIEWER_WEB_APPLICATION_H_

#include <granit/renderer/renderer.h>
#include <granit/renderer/swapchain.h>

namespace granit::example::model_viewer::web {

// 仓库私有的启动配置；回调在浏览器主线程执行，配置会复制保存到事件循环结束。
struct application_options {
  // 字符串在整个事件循环期间保持有效；回调可为空，失败结果会终止启动。
  const char* default_model_url{};
  granit_result (*renderer_ready)(granit_renderer, const granit_renderer_limits&){};
  granit_result (*presentation_ready)(granit_renderer, granit_swapchain,
                                      const granit_swapchain_info&){};
};

// 每个页面调用一次，启动共用的模型加载、输入和渲染循环。
int run_application(const application_options& options);

} // namespace granit::example::model_viewer::web

#endif
