// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_graph/graph_compiler.h"

#include <catch2/catch_all.hpp>

#include <vector>

using granit::render_graph::access_type;
using granit::render_graph::compile_error;
using granit::render_graph::graph_compiler;
using granit::render_graph::pass_desc;
using granit::render_graph::resource_access;
using granit::render_graph::resource_desc;

TEST_CASE("Render Graph 按资源依赖稳定排序") {
  graph_compiler graph;
  const auto source = graph.add_resource({.imported = true});
  const auto intermediate = graph.add_resource();
  const auto output = graph.add_resource({.exported = true});

  const auto first = graph.add_pass(
      {.accesses = {{source, access_type::read}, {intermediate, access_type::write}}});
  const auto second = graph.add_pass(
      {.accesses = {{intermediate, access_type::read}, {output, access_type::write}}});

  const auto result = graph.compile();
  REQUIRE(result.succeeded());
  REQUIRE(result.execution_order == std::vector{first, second});
  CHECK(result.resource_lifetimes[intermediate].first_use == 0);
  CHECK(result.resource_lifetimes[intermediate].last_use == 1);
}

TEST_CASE("Render Graph 保持无依赖 Pass 的声明顺序") {
  graph_compiler graph;
  const auto first = graph.add_pass({.side_effect = true, .accesses = {}});
  const auto second = graph.add_pass({.side_effect = true, .accesses = {}});
  const auto third = graph.add_pass({.side_effect = true, .accesses = {}});

  const auto result = graph.compile();
  REQUIRE(result.succeeded());
  CHECK(result.execution_order == std::vector{first, second, third});
}

TEST_CASE("Render Graph 裁剪不影响导出结果的 Pass") {
  graph_compiler graph;
  const auto kept_resource = graph.add_resource({.exported = true});
  const auto unused_resource = graph.add_resource();
  const auto kept = graph.add_pass({.accesses = {{kept_resource, access_type::write}}});
  static_cast<void>(graph.add_pass({.accesses = {{unused_resource, access_type::write}}}));

  const auto result = graph.compile();
  REQUIRE(result.succeeded());
  CHECK(result.execution_order == std::vector{kept});
  CHECK_FALSE(result.resource_lifetimes[unused_resource].used);
}

TEST_CASE("Render Graph 裁剪被覆盖的写入和无副作用读取") {
  graph_compiler graph;
  const auto resource = graph.add_resource({.imported = true, .exported = true});
  static_cast<void>(graph.add_pass({.accesses = {{resource, access_type::write}}}));
  static_cast<void>(graph.add_pass({.accesses = {{resource, access_type::read}}}));
  const auto final_writer = graph.add_pass({.accesses = {{resource, access_type::write}}});

  const auto result = graph.compile();
  REQUIRE(result.succeeded());
  CHECK(result.execution_order == std::vector{final_writer});
}

TEST_CASE("Render Graph 保留具有副作用的 Pass 及其生产者") {
  graph_compiler graph;
  const auto resource = graph.add_resource();
  const auto producer = graph.add_pass({.accesses = {{resource, access_type::write}}});
  const auto consumer =
      graph.add_pass({.side_effect = true, .accesses = {{resource, access_type::read}}});

  const auto result = graph.compile();
  REQUIRE(result.succeeded());
  CHECK(result.execution_order == std::vector{producer, consumer});
}

TEST_CASE("Render Graph 拒绝未初始化读取") {
  graph_compiler graph;
  const auto resource = graph.add_resource();
  const auto pass =
      graph.add_pass({.side_effect = true, .accesses = {{resource, access_type::read}}});

  const auto result = graph.compile();
  CHECK(result.error == compile_error::uninitialized_read);
  CHECK(result.error_pass == pass);
  CHECK(result.error_resource == resource);
}

TEST_CASE("Render Graph 检测显式依赖环") {
  graph_compiler graph;
  const auto first = graph.add_pass();
  const auto second = graph.add_pass();
  REQUIRE(graph.add_dependency(first, second));
  REQUIRE(graph.add_dependency(second, first));

  const auto result = graph.compile();
  CHECK(result.error == compile_error::cycle);
}

TEST_CASE("Render Graph 拒绝重复和越界资源访问") {
  SECTION("重复访问") {
    graph_compiler graph;
    const auto resource = graph.add_resource({.imported = true});
    static_cast<void>(graph.add_pass(
        {.side_effect = true,
         .accesses = {{resource, access_type::read}, {resource, access_type::write}}}));
    CHECK(graph.compile().error == compile_error::duplicate_access);
  }

  SECTION("越界访问") {
    graph_compiler graph;
    static_cast<void>(graph.add_pass({.side_effect = true, .accesses = {{42, access_type::read}}}));
    CHECK(graph.compile().error == compile_error::invalid_resource);
  }
}
