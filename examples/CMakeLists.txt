# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_version_example version.cpp)
target_link_libraries(granit_version_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_version_example)

add_executable(granit_renderer_example renderer.cpp)
target_link_libraries(granit_renderer_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_renderer_example)
