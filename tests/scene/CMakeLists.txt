# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS unit)

add_executable(
  granit_scene_test
  multi_view_submission_test.cpp
  scene_pbr_adapter_test.cpp
  scene_submission_test.cpp
  scene_visibility_test.cpp
)
target_include_directories(granit_scene_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_scene_test PRIVATE granit::scene Catch2::Catch2WithMain)
target_compile_options(granit_scene_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
granit_target_compile_warnings(granit_scene_test)
add_test(NAME granit.scene COMMAND granit_scene_test)

granit_target_output_directories(granit_scene_test)
