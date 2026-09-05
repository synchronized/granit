# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_consumer LANGUAGES C CXX)

include(CTest)

find_package(granit 0.11 CONFIG REQUIRED COMPONENTS RenderPipeline Window Input)

if(NOT granit_VERSION VERSION_EQUAL "0.11.0")
  message(FATAL_ERROR "Granit 包版本不符合预期: ${granit_VERSION}")
endif()

get_target_property(granit_core_linkage granit::granit TYPE)
get_target_property(granit_pipeline_linkage granit::render_pipeline TYPE)
get_target_property(granit_window_linkage granit::window TYPE)
get_target_property(granit_input_linkage granit::input TYPE)
if(NOT granit_core_linkage STREQUAL granit_pipeline_linkage OR
   NOT granit_core_linkage STREQUAL granit_window_linkage OR
   NOT granit_core_linkage STREQUAL granit_input_linkage)
  message(FATAL_ERROR "核心库、RenderPipeline、Window 与 Input component 的链接类型不一致")
endif()
if(granit_core_linkage STREQUAL "STATIC_LIBRARY")
  set(granit_consumer_linkage_definition GRANIT_CONSUMER_EXPECT_STATIC)
elseif(granit_core_linkage STREQUAL "SHARED_LIBRARY")
  set(granit_consumer_linkage_definition GRANIT_CONSUMER_EXPECT_SHARED)
else()
  message(FATAL_ERROR "无法验证未知的 Granit 链接类型: ${granit_core_linkage}")
endif()

add_executable(granit_c_consumer main.c)
target_link_libraries(granit_c_consumer PRIVATE granit::granit)
set_target_properties(granit_c_consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
target_compile_definitions(granit_c_consumer PRIVATE ${granit_consumer_linkage_definition})
target_compile_definitions(
  granit_c_consumer PRIVATE GRANIT_CONSUMER_PACKAGE_VERSION="${granit_VERSION}"
)

add_executable(granit_cpp_consumer main.cpp)
target_link_libraries(granit_cpp_consumer PRIVATE granit::granit)
target_compile_features(granit_cpp_consumer PRIVATE cxx_std_20)
target_compile_definitions(granit_cpp_consumer PRIVATE ${granit_consumer_linkage_definition})
target_compile_definitions(
  granit_cpp_consumer PRIVATE GRANIT_CONSUMER_PACKAGE_VERSION="${granit_VERSION}"
)

add_executable(granit_pipeline_c_consumer pipeline_main.c)
target_link_libraries(granit_pipeline_c_consumer PRIVATE granit::render_pipeline)
set_target_properties(
  granit_pipeline_c_consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
)
target_compile_definitions(
  granit_pipeline_c_consumer
  PRIVATE ${granit_consumer_linkage_definition} GRANIT_CONSUMER_CHECK_RENDER_PIPELINE
)

add_executable(granit_pipeline_cpp_consumer pipeline_main.cpp)
target_link_libraries(granit_pipeline_cpp_consumer PRIVATE granit::render_pipeline)
target_compile_features(granit_pipeline_cpp_consumer PRIVATE cxx_std_20)
target_compile_definitions(
  granit_pipeline_cpp_consumer
  PRIVATE ${granit_consumer_linkage_definition} GRANIT_CONSUMER_CHECK_RENDER_PIPELINE
)

add_executable(granit_window_c_consumer window_main.c)
target_link_libraries(granit_window_c_consumer PRIVATE granit::window)
set_target_properties(granit_window_c_consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)

add_executable(granit_input_c_consumer input_main.c)
target_link_libraries(granit_input_c_consumer PRIVATE granit::input)
set_target_properties(granit_input_c_consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)

add_executable(granit_input_cpp_consumer input_main.cpp)
target_link_libraries(granit_input_cpp_consumer PRIVATE granit::input)
target_compile_features(granit_input_cpp_consumer PRIVATE cxx_std_20)

foreach(
  granit_consumer_test
  IN ITEMS
    granit_c_consumer
    granit_cpp_consumer
    granit_pipeline_c_consumer
    granit_pipeline_cpp_consumer
    granit_window_c_consumer
    granit_input_c_consumer
    granit_input_cpp_consumer
)
  add_test(NAME ${granit_consumer_test} COMMAND ${granit_consumer_test})
  if(WIN32)
    set_property(
      TEST ${granit_consumer_test}
      PROPERTY ENVIRONMENT_MODIFICATION
               "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
    )
  elseif(APPLE)
    set_property(
      TEST ${granit_consumer_test}
      PROPERTY ENVIRONMENT_MODIFICATION
               "DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
    )
  else()
    set_property(
      TEST ${granit_consumer_test}
      PROPERTY ENVIRONMENT_MODIFICATION
               "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
    )
  endif()
endforeach()
