# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(
  granit_example_assets STATIC asset_store.cpp asset_store.h gltf_resolver.cpp gltf_resolver.h
)
target_compile_features(granit_example_assets PUBLIC cxx_std_20)
target_include_directories(granit_example_assets PUBLIC "${PROJECT_SOURCE_DIR}/examples/common")
target_link_libraries(granit_example_assets PUBLIC granit_example_gltf_support)
set_target_properties(granit_example_assets PROPERTIES FOLDER "Examples/Common")
granit_target_compile_warnings(granit_example_assets)

if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
  add_executable(granit_example_assets_test asset_store_test.cpp)
  target_link_libraries(
    granit_example_assets_test PRIVATE granit_example_assets Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_assets_test)
  add_test(NAME granit.example.assets COMMAND granit_example_assets_test)
endif()

function(granit_example_target_assets)
  set(one_value_args TARGET ROOT)
  set(multi_value_args FILES)
  cmake_parse_arguments(ARG "" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(NOT ARG_TARGET OR NOT TARGET "${ARG_TARGET}" OR NOT ARG_ROOT OR NOT ARG_FILES)
    message(FATAL_ERROR "granit_example_target_assets 参数不完整")
  endif()

  foreach(relative_path IN LISTS ARG_FILES)
    if(IS_ABSOLUTE "${relative_path}" OR relative_path MATCHES "(^|/)\\.\\.(/|$)")
      message(FATAL_ERROR "示例资产必须使用 ROOT 下的规范相对路径：${relative_path}")
    endif()
    set(source "${ARG_ROOT}/${relative_path}")
    if(NOT EXISTS "${source}")
      message(FATAL_ERROR "示例资产不存在：${source}")
    endif()

    if(EMSCRIPTEN)
      target_link_options(
        "${ARG_TARGET}" PRIVATE "--preload-file=${source}@/assets/${relative_path}"
      )
    else()
      get_filename_component(relative_directory "${relative_path}" DIRECTORY)
      set(destination "$<TARGET_FILE_DIR:${ARG_TARGET}>/assets/${relative_directory}")
      add_custom_command(
        TARGET "${ARG_TARGET}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${destination}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${destination}"
        VERBATIM
      )
    endif()
  endforeach()
endfunction()
