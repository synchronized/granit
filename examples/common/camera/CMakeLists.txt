# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(
  granit_example_camera STATIC
  orbit_camera.cpp
  orbit_camera.h
  orbit_camera_input.h
  orbit_camera_input_accumulator.cpp
  orbit_camera_input_accumulator.h
)
target_compile_features(granit_example_camera PUBLIC cxx_std_20)
target_include_directories(granit_example_camera PUBLIC "${PROJECT_SOURCE_DIR}/examples/common")
target_link_libraries(granit_example_camera PUBLIC granit::granit)
set_target_properties(granit_example_camera PROPERTIES FOLDER "Examples/Common")
granit_target_compile_warnings(granit_example_camera)

if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
  add_executable(
    granit_example_camera_test orbit_camera_test.cpp orbit_camera_input_accumulator_test.cpp
  )
  target_link_libraries(
    granit_example_camera_test PRIVATE granit_example_camera Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_camera_test)
  add_test(NAME granit.example.camera COMMAND granit_example_camera_test)
endif()
