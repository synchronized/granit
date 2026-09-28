# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(TARGET granit_example_imgui)
  add_library(granit_example_tutorial STATIC tutorial_runtime.cpp tutorial_runtime.h)
  target_compile_features(granit_example_tutorial PUBLIC cxx_std_20)
  target_include_directories(
    granit_example_tutorial PUBLIC "${PROJECT_SOURCE_DIR}/examples/common"
  )
  target_link_libraries(
    granit_example_tutorial PUBLIC granit_example_application granit_example_imgui
  )
  set_target_properties(granit_example_tutorial PROPERTIES FOLDER "Examples/Common")
  granit_target_compile_warnings(granit_example_tutorial)
endif()
