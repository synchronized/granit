# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_web_consumer LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "浏览器消费端测试需要 Emscripten 工具链")
endif()

include(CTest)
find_package(granit 0.21 CONFIG REQUIRED COMPONENTS RenderPipeline OPTIONAL_COMPONENTS Window Input)
if(granit_Window_FOUND OR granit_Input_FOUND)
  message(FATAL_ERROR "浏览器安装包不应包含未实现的原生 Window/Input 组件")
endif()
get_target_property(core_type granit::granit TYPE)
if(NOT core_type STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "浏览器安装包必须提供静态核心库")
endif()

# 复用仓库测试及其依赖，验证安装目标，不将测试框架引入安装导出。
get_filename_component(repository_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(GRANIT_BUILD_TESTING ON)
include("${repository_dir}/cmake/utility.cmake")
add_subdirectory("${repository_dir}/3rd" third_party)
add_subdirectory("${repository_dir}/tests/core" core)
add_executable(granit_web_pipeline_c_consumer pipeline_c_test.c)
target_compile_features(granit_web_pipeline_c_consumer PRIVATE c_std_11)
target_link_libraries(granit_web_pipeline_c_consumer PRIVATE granit::render_pipeline Unity::Unity)
set_target_properties(granit_web_pipeline_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
granit_target_compile_warnings(granit_web_pipeline_c_consumer)
add_test(NAME granit.web_consumer.pipeline COMMAND granit_web_pipeline_c_consumer)
