// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_graph/graph_compiler.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_set>

namespace granit::render_graph {
namespace {

using adjacency_list = std::vector<std::vector<pass_id>>;

void add_edge(adjacency_list& outgoing, adjacency_list& incoming, pass_id before, pass_id after) {
  auto& destinations = outgoing[before];
  if (std::find(destinations.begin(), destinations.end(), after) != destinations.end()) {
    return;
  }
  destinations.push_back(after);
  incoming[after].push_back(before);
}

bool contains_cycle(const adjacency_list& outgoing, const adjacency_list& incoming) {
  std::vector<std::size_t> indegrees(incoming.size());
  std::queue<pass_id> ready;
  for (std::size_t pass_index = 0; pass_index < incoming.size(); ++pass_index) {
    indegrees[pass_index] = incoming[pass_index].size();
    if (indegrees[pass_index] == 0) {
      ready.push(static_cast<pass_id>(pass_index));
    }
  }

  std::size_t visited = 0;
  while (!ready.empty()) {
    const auto pass = ready.front();
    ready.pop();
    ++visited;
    for (const auto destination : outgoing[pass]) {
      --indegrees[destination];
      if (indegrees[destination] == 0) {
        ready.push(destination);
      }
    }
  }
  return visited != outgoing.size();
}

compile_result make_error(compile_error error, pass_id pass = invalid_pass_id,
                          resource_id resource = invalid_resource_id) {
  compile_result result;
  result.error = error;
  result.error_pass = pass;
  result.error_resource = resource;
  return result;
}

} // namespace

resource_id graph_compiler::add_resource(resource_desc desc) {
  const auto id = static_cast<resource_id>(resources_.size());
  resources_.push_back(desc);
  return id;
}

pass_id graph_compiler::add_pass(pass_desc desc) {
  const auto id = static_cast<pass_id>(passes_.size());
  passes_.push_back(std::move(desc));
  return id;
}

bool graph_compiler::add_dependency(pass_id before, pass_id after) {
  if (before >= passes_.size() || after >= passes_.size()) {
    return false;
  }
  explicit_dependencies_.push_back({before, after});
  return true;
}

compile_result graph_compiler::compile() const {
  adjacency_list outgoing(passes_.size());
  adjacency_list incoming(passes_.size());
  adjacency_list requirements(passes_.size());
  std::vector<pass_id> last_writers(resources_.size(), invalid_pass_id);
  std::vector<std::vector<pass_id>> readers(resources_.size());

  for (const auto& dependency : explicit_dependencies_) {
    if (dependency.before >= passes_.size() || dependency.after >= passes_.size()) {
      return make_error(compile_error::invalid_pass);
    }
    add_edge(outgoing, incoming, dependency.before, dependency.after);
    requirements[dependency.after].push_back(dependency.before);
  }

  for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
    const auto current_pass = static_cast<pass_id>(pass_index);
    std::unordered_set<resource_id> accessed_resources;

    for (const auto& access : passes_[pass_index].accesses) {
      if (access.resource >= resources_.size()) {
        return make_error(compile_error::invalid_resource, current_pass, access.resource);
      }
      if (!accessed_resources.insert(access.resource).second) {
        return make_error(compile_error::duplicate_access, current_pass, access.resource);
      }

      const auto last_writer = last_writers[access.resource];
      const bool reads = access.type == access_type::read || access.type == access_type::read_write;
      const bool writes =
          access.type == access_type::write || access.type == access_type::read_write;

      if (reads) {
        if (last_writer == invalid_pass_id && !resources_[access.resource].imported) {
          return make_error(compile_error::uninitialized_read, current_pass, access.resource);
        }
        if (last_writer != invalid_pass_id) {
          add_edge(outgoing, incoming, last_writer, current_pass);
          requirements[current_pass].push_back(last_writer);
        }
      }

      if (writes) {
        if (last_writer != invalid_pass_id) {
          add_edge(outgoing, incoming, last_writer, current_pass);
        }
        for (const auto reader : readers[access.resource]) {
          add_edge(outgoing, incoming, reader, current_pass);
        }
        readers[access.resource].clear();
        last_writers[access.resource] = current_pass;
      } else {
        readers[access.resource].push_back(current_pass);
      }
    }
  }

  if (contains_cycle(outgoing, incoming)) {
    return make_error(compile_error::cycle);
  }

  std::vector<bool> required(passes_.size(), false);
  std::vector<pass_id> pending;
  for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
    if (passes_[pass_index].side_effect) {
      pending.push_back(static_cast<pass_id>(pass_index));
    }
  }
  for (std::size_t resource_index = 0; resource_index < resources_.size(); ++resource_index) {
    if (resources_[resource_index].exported && last_writers[resource_index] != invalid_pass_id) {
      pending.push_back(last_writers[resource_index]);
    }
  }
  while (!pending.empty()) {
    const auto pass = pending.back();
    pending.pop_back();
    if (required[pass]) {
      continue;
    }
    required[pass] = true;
    pending.insert(pending.end(), requirements[pass].begin(), requirements[pass].end());
  }

  std::vector<std::uint32_t> indegrees(passes_.size(), 0);
  std::priority_queue<pass_id, std::vector<pass_id>, std::greater<>> ready;
  for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
    if (!required[pass_index]) {
      continue;
    }
    for (const auto dependency : incoming[pass_index]) {
      if (required[dependency]) {
        ++indegrees[pass_index];
      }
    }
    if (indegrees[pass_index] == 0) {
      ready.push(static_cast<pass_id>(pass_index));
    }
  }

  compile_result result;
  result.resource_lifetimes.resize(resources_.size());
  for (std::size_t pass_index = 0; pass_index < incoming.size(); ++pass_index) {
    if (!required[pass_index]) {
      continue;
    }
    for (const auto dependency : incoming[pass_index]) {
      if (required[dependency]) {
        result.dependencies.push_back({dependency, static_cast<pass_id>(pass_index)});
      }
    }
  }
  while (!ready.empty()) {
    const auto pass = ready.top();
    ready.pop();
    result.execution_order.push_back(pass);
    for (const auto destination : outgoing[pass]) {
      if (!required[destination]) {
        continue;
      }
      --indegrees[destination];
      if (indegrees[destination] == 0) {
        ready.push(destination);
      }
    }
  }

  for (std::size_t order_index = 0; order_index < result.execution_order.size(); ++order_index) {
    const auto pass = result.execution_order[order_index];
    for (const auto& access : passes_[pass].accesses) {
      auto& lifetime = result.resource_lifetimes[access.resource];
      const auto position = static_cast<std::uint32_t>(order_index);
      if (!lifetime.used) {
        lifetime.used = true;
        lifetime.first_use = position;
      }
      lifetime.last_use = position;
    }
  }

  return result;
}

} // namespace granit::render_graph
