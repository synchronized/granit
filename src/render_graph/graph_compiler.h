// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_GRAPH_GRAPH_COMPILER_H
#define GRANIT_RENDER_GRAPH_GRAPH_COMPILER_H

#include <cstdint>
#include <limits>
#include <vector>

namespace granit::render_graph {

using resource_id = std::uint32_t;
using pass_id = std::uint32_t;

inline constexpr resource_id invalid_resource_id = std::numeric_limits<resource_id>::max();
inline constexpr pass_id invalid_pass_id = std::numeric_limits<pass_id>::max();

enum class access_type : std::uint8_t {
  read,
  write,
  read_write,
};

enum class compile_error : std::uint8_t {
  none,
  invalid_resource,
  invalid_pass,
  duplicate_access,
  uninitialized_read,
  cycle,
};

struct resource_desc {
  bool imported = false;
  bool exported = false;
};

struct resource_access {
  resource_id resource = invalid_resource_id;
  access_type type = access_type::read;
};

struct pass_desc {
  bool side_effect = false;
  std::vector<resource_access> accesses;
};

struct resource_lifetime {
  bool used = false;
  std::uint32_t first_use = 0;
  std::uint32_t last_use = 0;
};

struct dependency_edge {
  pass_id before = invalid_pass_id;
  pass_id after = invalid_pass_id;
};

struct compile_result {
  compile_error error = compile_error::none;
  pass_id error_pass = invalid_pass_id;
  resource_id error_resource = invalid_resource_id;
  std::vector<pass_id> execution_order;
  std::vector<dependency_edge> dependencies;
  std::vector<resource_lifetime> resource_lifetimes;

  [[nodiscard]] bool succeeded() const noexcept { return error == compile_error::none; }
};

class graph_compiler {
public:
  [[nodiscard]] resource_id add_resource(resource_desc desc = {});
  [[nodiscard]] pass_id add_pass(pass_desc desc = {});
  [[nodiscard]] bool add_dependency(pass_id before, pass_id after);
  [[nodiscard]] compile_result compile() const;

private:
  struct dependency {
    pass_id before = invalid_pass_id;
    pass_id after = invalid_pass_id;
  };

  std::vector<resource_desc> resources_;
  std::vector<pass_desc> passes_;
  std::vector<dependency> explicit_dependencies_;
};

} // namespace granit::render_graph

#endif
