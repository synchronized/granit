// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_BROWSER_TEST_HOOKS_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_BROWSER_TEST_HOOKS_H_

#include <granit/renderer/renderer.h>
#include <granit/renderer/swapchain.h>

namespace granit::example::model_viewer::web::browser_test {

/** browser-platform-smoke 注入的 C ABI 验收钩子；正式 Model Viewer 不编译这些调用。 */
struct hooks {
  granit_result (*renderer_ready)(granit_renderer, const granit_renderer_limits&){};
  granit_result (*presentation_ready)(granit_renderer, granit_swapchain,
                                      const granit_swapchain_info&){};
};

void configure(hooks value) noexcept;

} // namespace granit::example::model_viewer::web::browser_test

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_BROWSER_TEST_HOOKS_H_
