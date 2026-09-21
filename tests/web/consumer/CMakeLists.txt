# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_web_consumer LANGUAGES C CXX)

set_property(DIRECTORY APPEND PROPERTY LABELS browser package)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "浏览器消费端测试需要 Emscripten 工具链")
endif()

include(CTest)
find_package(granit 0.25 CONFIG REQUIRED COMPONENTS RenderPipeline Window)
get_target_property(core_type granit::granit TYPE)
if(NOT core_type STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "浏览器安装包必须提供静态核心库")
endif()

# 仅复用 Unity；消费端只验证安装目标，不引入源码级内部测试。
get_filename_component(repository_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
include("${repository_dir}/cmake/granit_utility.cmake")
include("${repository_dir}/3rd/unity.cmake")
add_executable(granit_web_pipeline_c_consumer pipeline_c_test.c)
target_compile_features(granit_web_pipeline_c_consumer PRIVATE c_std_11)
target_link_libraries(
  granit_web_pipeline_c_consumer PRIVATE granit::render_pipeline granit::window Unity::Unity
)
set_target_properties(granit_web_pipeline_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
granit_target_compile_warnings(granit_web_pipeline_c_consumer)
add_test(NAME granit.web_consumer.pipeline COMMAND granit_web_pipeline_c_consumer)
