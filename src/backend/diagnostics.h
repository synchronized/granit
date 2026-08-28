// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_DIAGNOSTICS_H_
#define GRANIT_BACKEND_DIAGNOSTICS_H_

namespace granit::detail {

class diagnostic_sink;

/** 向 Registry 提供后端中立的校验开关和诊断输出。 */
class backend_diagnostic_renderer {
public:
  backend_diagnostic_renderer() = default;
  virtual ~backend_diagnostic_renderer() = default;
  backend_diagnostic_renderer(const backend_diagnostic_renderer&) = delete;
  backend_diagnostic_renderer& operator=(const backend_diagnostic_renderer&) = delete;

  [[nodiscard]] virtual bool validation_enabled() const noexcept = 0;
  [[nodiscard]] virtual const diagnostic_sink& diagnostics() const noexcept = 0;
};

} // namespace granit::detail

#endif
