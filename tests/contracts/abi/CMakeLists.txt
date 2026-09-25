# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(granit_abi_layout_test OBJECT layout_h.c)
target_compile_features(granit_abi_layout_test PRIVATE c_std_11)
target_link_libraries(
  granit_abi_layout_test
  PRIVATE granit::granit granit::render_pipeline granit::window granit::asset_tools
)
granit_target_compile_warnings(granit_abi_layout_test)

# 稳定候选布局按 component 独立编译，避免聚合头让某个 component 的漂移被其他依赖掩盖。
add_library(granit_abi_core_layout_test OBJECT core_layout_h.c)
target_compile_features(granit_abi_core_layout_test PRIVATE c_std_11)
target_link_libraries(granit_abi_core_layout_test PRIVATE granit::granit)
granit_target_compile_warnings(granit_abi_core_layout_test)

add_library(granit_abi_window_layout_test OBJECT window_layout_h.c)
target_compile_features(granit_abi_window_layout_test PRIVATE c_std_11)
target_link_libraries(granit_abi_window_layout_test PRIVATE granit::window)
granit_target_compile_warnings(granit_abi_window_layout_test)

add_library(granit_abi_render_pipeline_layout_test OBJECT render_pipeline_layout_h.c)
target_compile_features(granit_abi_render_pipeline_layout_test PRIVATE c_std_11)
target_link_libraries(
  granit_abi_render_pipeline_layout_test PRIVATE granit::render_pipeline
)
granit_target_compile_warnings(granit_abi_render_pipeline_layout_test)

if(BUILD_SHARED_LIBS)
  add_executable(granit_abi_exports_test export_symbols_test.cpp)
  target_compile_features(granit_abi_exports_test PRIVATE cxx_std_20)
  target_compile_definitions(
    granit_abi_exports_test
    PRIVATE
      GRANIT_ABI_CORE_LIBRARY_PATH="$<TARGET_FILE:granit>"
      GRANIT_ABI_RENDER_PIPELINE_LIBRARY_PATH="$<TARGET_FILE:granit_render_pipeline>"
      GRANIT_ABI_WINDOW_LIBRARY_PATH="$<TARGET_FILE:granit_window>"
      GRANIT_ABI_ASSET_TOOLS_LIBRARY_PATH="$<TARGET_FILE:granit_asset_tools>"
      GRANIT_ABI_CORE_SYMBOLS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/snapshots/0.35.0/core_symbols.txt"
      GRANIT_ABI_RENDER_PIPELINE_SYMBOLS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/snapshots/0.35.0/render_pipeline_symbols.txt"
      GRANIT_ABI_WINDOW_SYMBOLS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/snapshots/0.35.0/window_symbols.txt"
      GRANIT_ABI_ASSET_TOOLS_SYMBOLS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/snapshots/0.35.0/asset_tools_symbols.txt"
  )
  target_include_directories(
    granit_abi_exports_test
    PRIVATE "$<TARGET_PROPERTY:granit,INTERFACE_INCLUDE_DIRECTORIES>"
  )
  target_link_libraries(
    granit_abi_exports_test
    PRIVATE Catch2::Catch2WithMain ${CMAKE_DL_LIBS}
  )
  add_dependencies(
    granit_abi_exports_test granit granit_render_pipeline granit_window granit_asset_tools
  )
  granit_target_compile_warnings(granit_abi_exports_test)
  granit_target_output_directories(granit_abi_exports_test)
  add_test(NAME granit.abi.exports COMMAND granit_abi_exports_test)

  if(MSVC)
    set(granit_abi_export_tool "${CMAKE_DUMPBIN}")
    if(NOT granit_abi_export_tool AND CMAKE_LINKER)
      get_filename_component(granit_msvc_tool_directory "${CMAKE_LINKER}" DIRECTORY)
      set(granit_abi_export_tool "${granit_msvc_tool_directory}/dumpbin.exe")
    endif()
    set(granit_abi_export_tool_kind dumpbin)
  elseif(UNIX)
    set(granit_abi_export_tool "${CMAKE_NM}")
    set(granit_abi_export_tool_kind nm)
  endif()

  if(granit_abi_export_tool)
    function(granit_add_abi_export_snapshot_test component target snapshot)
      add_test(
        NAME "granit.abi.exports.${component}"
        COMMAND
          "${CMAKE_COMMAND}"
          "-DGRANIT_ABI_COMPONENT=${component}"
          "-DGRANIT_ABI_LIBRARY=$<TARGET_FILE:${target}>"
          "-DGRANIT_ABI_SNAPSHOT=${CMAKE_CURRENT_SOURCE_DIR}/snapshots/0.35.0/${snapshot}"
          "-DGRANIT_ABI_EXPORT_TOOL=${granit_abi_export_tool}"
          "-DGRANIT_ABI_EXPORT_TOOL_KIND=${granit_abi_export_tool_kind}"
          "-DGRANIT_ABI_PRIVATE_EXPORTS=${ARGN}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/check_exported_symbols.cmake"
      )
    endfunction()

    granit_add_abi_export_snapshot_test(core granit core_symbols.txt)
    granit_add_abi_export_snapshot_test(
      render_pipeline granit_render_pipeline render_pipeline_symbols.txt
      "${CMAKE_CURRENT_SOURCE_DIR}/snapshots/0.1.0/render_pipeline_private_symbols.txt"
    )
    add_test(
      NAME granit.abi.exports.window
      COMMAND
        "${CMAKE_COMMAND}"
        "-DGRANIT_ABI_COMPONENT=window"
        "-DGRANIT_ABI_LIBRARY=$<TARGET_FILE:granit_window>"
        "-DGRANIT_ABI_SNAPSHOT=${CMAKE_CURRENT_SOURCE_DIR}/snapshots/0.35.0/window_symbols.txt"
        "-DGRANIT_ABI_EXPORT_TOOL=${granit_abi_export_tool}"
        "-DGRANIT_ABI_EXPORT_TOOL_KIND=${granit_abi_export_tool_kind}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_exported_symbols.cmake"
    )
    granit_add_abi_export_snapshot_test(
      asset_tools granit_asset_tools asset_tools_symbols.txt
    )
  endif()

  set_property(TEST granit.abi.exports APPEND PROPERTY LABELS package release)
  if(granit_abi_export_tool)
    set_property(
      TEST
        granit.abi.exports.core
        granit.abi.exports.render_pipeline
        granit.abi.exports.window
        granit.abi.exports.asset_tools
      APPEND PROPERTY LABELS package release
    )
  endif()
endif()

function(granit_add_abi_compatibility_test component baseline policy)
  set(options)
  set(one_value_args ALLOWED_REMOVALS)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "" ${ARGN})
  add_test(
    NAME "granit.abi.compatibility.${component}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DGRANIT_ABI_COMPONENT=${component}"
      "-DGRANIT_ABI_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/snapshots/${baseline}/${component}_symbols.txt"
      "-DGRANIT_ABI_CURRENT=${CMAKE_CURRENT_SOURCE_DIR}/snapshots/0.35.0/${component}_symbols.txt"
      "-DGRANIT_ABI_POLICY=${policy}"
      "-DGRANIT_ABI_ALLOWED_REMOVALS=${ARG_ALLOWED_REMOVALS}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/check_symbol_compatibility.cmake"
  )
  set_property(TEST "granit.abi.compatibility.${component}" APPEND PROPERTY LABELS package release)
endfunction()

granit_add_abi_compatibility_test(core 0.29.0 compatible)
granit_add_abi_compatibility_test(render_pipeline 0.29.0 compatible)
granit_add_abi_compatibility_test(window 0.25.0 compatible)
granit_add_abi_compatibility_test(asset_tools 0.34.0 experimental)
