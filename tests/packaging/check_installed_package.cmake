# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_INSTALL_PREFIX OR
   NOT DEFINED GRANIT_TEST_BINARY_DIR)
  message(FATAL_ERROR "必须提供 GRANIT_SOURCE_DIR、GRANIT_INSTALL_PREFIX 和 GRANIT_TEST_BINARY_DIR")
endif()

if(NOT DEFINED GRANIT_TEST_CONFIGURATION)
  set(GRANIT_TEST_CONFIGURATION Release)
endif()

# 从根 CMakeLists.txt 读取当前版本，派生当前 minor、精确版本与下一 minor。
file(READ "${GRANIT_SOURCE_DIR}/CMakeLists.txt" granit_root_cmake)
string(
  REGEX MATCH
  "project\\([ \t\r\n]*granit[ \t\r\n]+VERSION[ \t\r\n]+([0-9]+)\\.([0-9]+)\\.([0-9]+)"
  granit_project_match
  "${granit_root_cmake}"
)
if(NOT granit_project_match)
  message(FATAL_ERROR "无法从根 CMakeLists.txt 读取 Granit 版本")
endif()
set(granit_current_minor "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
set(granit_current_version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
math(EXPR granit_next_minor_number "${CMAKE_MATCH_2} + 1")
set(granit_next_minor "${CMAKE_MATCH_1}.${granit_next_minor_number}")

function(granit_check_package name expected_success)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}"
      -S "${GRANIT_SOURCE_DIR}/tests/packaging/component_selection"
      -B "${GRANIT_TEST_BINARY_DIR}/${name}"
      "-DCMAKE_PREFIX_PATH=${GRANIT_INSTALL_PREFIX}"
      "-DCMAKE_BUILD_TYPE=${GRANIT_TEST_CONFIGURATION}"
      ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(expected_success AND NOT result EQUAL 0)
    message(FATAL_ERROR "${name} 应配置成功，但返回 ${result}\n${output}\n${error}")
  endif()
  if(NOT expected_success AND result EQUAL 0)
    message(FATAL_ERROR "${name} 应配置失败，但意外成功")
  endif()
  if(expected_success)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${GRANIT_TEST_BINARY_DIR}/${name}"
              --config "${GRANIT_TEST_CONFIGURATION}"
      RESULT_VARIABLE build_result
      OUTPUT_VARIABLE build_output
      ERROR_VARIABLE build_error
    )
    if(NOT build_result EQUAL 0)
      message(FATAL_ERROR
              "${name} 应构建成功，但返回 ${build_result}\n${build_output}\n${build_error}")
    endif()
  endif()
endfunction()

granit_check_package(core_only TRUE -DGRANIT_REQUEST_VERSION=${granit_current_minor})
granit_check_package(render_pipeline TRUE -DGRANIT_REQUEST_VERSION=${granit_current_minor}
                     -DGRANIT_REQUEST_COMPONENT=RenderPipeline)
granit_check_package(window TRUE -DGRANIT_REQUEST_VERSION=${granit_current_minor}
                     -DGRANIT_REQUEST_COMPONENT=Window)
if(EXISTS "${GRANIT_INSTALL_PREFIX}/lib/cmake/granit/granitAssetToolsTargets.cmake")
  granit_check_package(asset_tools TRUE -DGRANIT_REQUEST_VERSION=${granit_current_minor}
                       -DGRANIT_REQUEST_COMPONENT=AssetTools)
else()
  granit_check_package(asset_tools_unavailable FALSE -DGRANIT_REQUEST_VERSION=${granit_current_minor}
                       -DGRANIT_REQUEST_COMPONENT=AssetTools)
endif()
granit_check_package(older_0_7 FALSE -DGRANIT_REQUEST_VERSION=0.7)
granit_check_package(older_0_8 FALSE -DGRANIT_REQUEST_VERSION=0.8)
granit_check_package(older_0_9 FALSE -DGRANIT_REQUEST_VERSION=0.9)
granit_check_package(older_0_10 FALSE -DGRANIT_REQUEST_VERSION=0.10)
granit_check_package(older_0_6 FALSE -DGRANIT_REQUEST_VERSION=0.6)
granit_check_package(older_0_4 FALSE -DGRANIT_REQUEST_VERSION=0.4)
granit_check_package(older_0_1 FALSE -DGRANIT_REQUEST_VERSION=0.1)
granit_check_package(older_0_11 FALSE -DGRANIT_REQUEST_VERSION=0.11)
granit_check_package(older_0_12 FALSE -DGRANIT_REQUEST_VERSION=0.12)
granit_check_package(older_0_13 FALSE -DGRANIT_REQUEST_VERSION=0.13)
granit_check_package(older_0_14 FALSE -DGRANIT_REQUEST_VERSION=0.14)
granit_check_package(older_0_15 FALSE -DGRANIT_REQUEST_VERSION=0.15)
granit_check_package(older_0_16 FALSE -DGRANIT_REQUEST_VERSION=0.16)
granit_check_package(older_0_17 FALSE -DGRANIT_REQUEST_VERSION=0.17)
granit_check_package(older_0_18 FALSE -DGRANIT_REQUEST_VERSION=0.18)
granit_check_package(older_0_19 FALSE -DGRANIT_REQUEST_VERSION=0.19)
granit_check_package(older_0_20 FALSE -DGRANIT_REQUEST_VERSION=0.20)
granit_check_package(older_0_21 FALSE -DGRANIT_REQUEST_VERSION=0.21)
granit_check_package(older_0_22 FALSE -DGRANIT_REQUEST_VERSION=0.22)
granit_check_package(older_0_23 FALSE -DGRANIT_REQUEST_VERSION=0.23)
granit_check_package(older_0_24 FALSE -DGRANIT_REQUEST_VERSION=0.24)
granit_check_package(exact TRUE -DGRANIT_REQUEST_VERSION=${granit_current_version} -DGRANIT_REQUEST_EXACT=ON)
granit_check_package(newer_minor FALSE -DGRANIT_REQUEST_VERSION=${granit_next_minor})
granit_check_package(incompatible_major FALSE -DGRANIT_REQUEST_VERSION=1.0)
granit_check_package(unknown_component FALSE -DGRANIT_REQUEST_COMPONENT=Unknown)
granit_check_package(removed_input FALSE -DGRANIT_REQUEST_COMPONENT=Input)
granit_check_package(removed_shader_tools FALSE -DGRANIT_REQUEST_COMPONENT=ShaderTools)

message(STATUS
        "安装包选包检查通过：Core 隔离、独立 component、0.x 次版本和未知 component 均符合预期")
