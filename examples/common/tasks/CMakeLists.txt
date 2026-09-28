# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(
  granit_example_tasks STATIC
  executor.cpp
  executor.h
  task_system.cpp
  task_system.h
)
target_compile_features(granit_example_tasks PUBLIC cxx_std_20)
target_include_directories(granit_example_tasks PUBLIC "${PROJECT_SOURCE_DIR}/examples/common")
target_link_libraries(granit_example_tasks PUBLIC granit::granit)
set_target_properties(granit_example_tasks PROPERTIES FOLDER "Examples/Common")
granit_target_compile_warnings(granit_example_tasks)

if(GRANIT_TESTING_ENABLED)
  add_executable(granit_example_tasks_test task_system_test.cpp)
  target_link_libraries(
    granit_example_tasks_test PRIVATE granit_example_tasks Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_tasks_test)
  add_test(NAME granit.example.tasks COMMAND granit_example_tasks_test)
endif()
