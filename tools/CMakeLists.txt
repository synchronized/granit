# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS integration tooling)

find_package(Threads REQUIRED)

if(TARGET granit_asset_tools)
  add_executable(
    granit_asset_tool
    granit_asset_tool/file_io.cpp
    granit_asset_tool/file_io.h
    granit_asset_tool/main.cpp
    granit_asset_tool/environment/commands.cpp
    granit_asset_tool/environment/commands.h
    granit_asset_tool/material/commands.cpp
    granit_asset_tool/material/commands.h
    granit_asset_tool/shader/arguments.cpp
    granit_asset_tool/shader/arguments.h
    granit_asset_tool/shader/commands.h
    granit_asset_tool/shader/commands/compile.cpp
    granit_asset_tool/shader/commands/inspect.cpp
    granit_asset_tool/shader/commands/library.cpp
    granit_asset_tool/shader/commands/targets.cpp
    granit_asset_tool/texture/commands.cpp
    granit_asset_tool/texture/commands.h
  )
  target_compile_features(granit_asset_tool PRIVATE cxx_std_20)
  target_include_directories(granit_asset_tool PRIVATE "${PROJECT_SOURCE_DIR}/tools")
  target_link_libraries(granit_asset_tool PRIVATE granit::asset_tools)
  target_compile_options(
    granit_asset_tool
    PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
  )
  granit_target_compile_warnings(granit_asset_tool)
  granit_target_output_directories(granit_asset_tool)
  set_target_properties(granit_asset_tool PROPERTIES FOLDER "Tools")
  if(UNIX AND NOT APPLE)
    set_target_properties(
      granit_asset_tool PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
  endif()
  if(GRANIT_BUILD_TOOLS)
    install(
      TARGETS granit_asset_tool
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT AssetTools
    )
  endif()
endif()

if(GRANIT_TESTING_ENABLED OR GRANIT_BUILD_BENCHMARKS)
  add_executable(
    granit_shader_fixture_tool
    "${PROJECT_SOURCE_DIR}/tests/support/shader_fixture_tool/main.cpp"
    "${PROJECT_SOURCE_DIR}/tests/support/shader_fixture_tool/commands.h"
    granit_asset_tool/shader/arguments.cpp
    granit_asset_tool/shader/arguments.h
    "${PROJECT_SOURCE_DIR}/tests/support/shader_fixture_tool/commands/library.cpp"
    "${PROJECT_SOURCE_DIR}/tests/support/shader_fixture_tool/commands/object.cpp"
    "${PROJECT_SOURCE_DIR}/src/asset_tools/shader/library_builder.cpp"
  )
  target_sources(
    granit_shader_fixture_tool
    PRIVATE
      $<TARGET_OBJECTS:granit_asset_format_shader>
      $<TARGET_OBJECTS:granit_asset_tools_shader_object_storage>
      $<TARGET_OBJECTS:granit_asset_tools_shader_compiler>
      $<TARGET_OBJECTS:granit_spirv_reflect>
  )
  target_compile_features(granit_shader_fixture_tool PRIVATE cxx_std_20)
  target_include_directories(
    granit_shader_fixture_tool
    PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/tests/support" "${PROJECT_SOURCE_DIR}/tools"
  )
  target_link_libraries(granit_shader_fixture_tool PRIVATE Threads::Threads)
  target_compile_options(
    granit_shader_fixture_tool
    PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
  )
  granit_target_compile_warnings(granit_shader_fixture_tool)
  granit_target_output_directories(granit_shader_fixture_tool)
  set_target_properties(granit_shader_fixture_tool PROPERTIES FOLDER "Tests/Tools")

endif()
