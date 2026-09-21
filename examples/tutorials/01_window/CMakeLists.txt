# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_tutorial_01_window main.cpp)

target_compile_features(granit_tutorial_01_window PRIVATE cxx_std_20)
target_link_libraries(granit_tutorial_01_window PRIVATE granit::granit granit::window)
granit_target_compile_warnings(granit_tutorial_01_window)
granit_target_output_directories(granit_tutorial_01_window)
