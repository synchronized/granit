# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(granit_example_application STATIC application.cpp application.h)

target_compile_features(granit_example_application PUBLIC cxx_std_20)
target_include_directories(
  granit_example_application
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/common"
)
target_link_libraries(granit_example_application PUBLIC granit::granit granit::window)

set_target_properties(granit_example_application PROPERTIES FOLDER "Examples/Common")
granit_target_compile_warnings(granit_example_application)
