// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_CHILD_PROCESS_H_
#define GRANIT_TOOLS_CHILD_PROCESS_H_

#include <string>
#include <vector>

namespace granit::tools {

struct process_result {
  int exit_code{};
  std::string standard_output;
  std::string standard_error;
};

/** 直接启动子进程并捕获输出，不经过平台 Shell。 */
bool run_process(const std::vector<std::string>& arguments, process_result& result);

} // namespace granit::tools

#endif
