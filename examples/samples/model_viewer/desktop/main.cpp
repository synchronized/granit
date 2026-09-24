// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/desktop/application.h"
#include "model_viewer/desktop/desktop_options.h"

#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void print_usage() {
  std::cerr << "用法：granit_sample_model_viewer --asset <文件> "
               "[--environment <文件.grenv>] "
               "[--backend=auto|vulkan] "
               "[--validation] [--smoke-test] [--no-ui] "
               "[--present-mode=fifo|immediate] [--profile-output <文件.json>]\n";
}

} // namespace

int main(int argc, char** argv) {
  std::vector<std::string_view> arguments;
  arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int index = 1; index < argc; ++index)
    arguments.emplace_back(argv[index]);

  granit::example::model_viewer::desktop::options options;
  const auto result = granit::example::model_viewer::desktop::parse_options(arguments, options);
  if (result.failed()) {
    print_usage();
    return 1;
  }
  return granit::example::model_viewer::desktop::application(std::move(options)).run();
}
