# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_render_graph_example LANGUAGES CXX)

find_package(granit CONFIG REQUIRED COMPONENTS RenderGraph)

add_executable(granit_render_graph_example main.cpp)
target_compile_features(granit_render_graph_example PRIVATE cxx_std_20)
target_link_libraries(granit_render_graph_example PRIVATE granit::render_graph)
