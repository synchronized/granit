# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS unit)

add_executable(
  granit_render_graph_test
  graph_compiler_test.cpp
  serial_graph_test.cpp
)
target_include_directories(granit_render_graph_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_render_graph_test
  PRIVATE granit::render_graph Catch2::Catch2WithMain $<$<PLATFORM_ID:Windows>:user32>
)
target_compile_options(
  granit_render_graph_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_render_graph_test)
add_test(NAME granit.render_graph COMMAND granit_render_graph_test)

granit_target_output_directories(granit_render_graph_test)
