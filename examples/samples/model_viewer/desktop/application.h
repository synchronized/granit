// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_DESKTOP_APPLICATION_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_DESKTOP_APPLICATION_H_

#include "model_viewer/desktop/desktop_options.h"

namespace granit::example::model_viewer::desktop {

/** Desktop Model Viewer 的主线程生命周期与状态推进。 */
class application {
public:
  explicit application(options options);

  [[nodiscard]] int run();

private:
  options options_;
};

} // namespace granit::example::model_viewer::desktop

#endif
