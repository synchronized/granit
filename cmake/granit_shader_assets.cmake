# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

function(granit_add_test_shader_object_from_payloads)
  set(options)
  set(one_value_args NAME SPIRV WGSL ENTRY STAGE OUTPUT_DIR OUTPUT_VAR)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SPIRV OR NOT ARG_WGSL OR NOT ARG_ENTRY OR NOT ARG_STAGE OR
     NOT ARG_OUTPUT_DIR)
    message(FATAL_ERROR "granit_add_test_shader_object_from_payloads 缺少必要参数")
  endif()

  set(object "${ARG_OUTPUT_DIR}/${ARG_NAME}.grshaderobj")
  add_custom_command(
    OUTPUT "${object}" "${object}.spv" "${object}.wgsl"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_OUTPUT_DIR}"
    COMMAND
      "$<TARGET_FILE:granit_shader_fixture_tool>" object --spirv "${ARG_SPIRV}" --wgsl "${ARG_WGSL}"
      --entry "${ARG_ENTRY}" --stage "${ARG_STAGE}" --output "${object}"
    DEPENDS granit_shader_fixture_tool "${ARG_SPIRV}" "${ARG_WGSL}"
    COMMENT "生成 Shader Object ${ARG_NAME}.grshaderobj"
    VERBATIM
  )
  set(${ARG_OUTPUT_VAR} "${object};${object}.spv;${object}.wgsl" PARENT_SCOPE)
endfunction()

function(granit_add_test_shader_object_from_hlsl)
  set(options)
  set(one_value_args NAME SOURCE ENTRY STAGE OUTPUT_DIR OUTPUT_VAR)
  set(multi_value_args DEFINES)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SOURCE OR NOT ARG_ENTRY OR NOT ARG_STAGE OR NOT ARG_OUTPUT_DIR)
    message(FATAL_ERROR "granit_add_test_shader_object_from_hlsl 缺少必要参数")
  endif()
  if(NOT GRANIT_SHADER_TOOLCHAIN_ROOT OR NOT GRANIT_DXC_EXECUTABLE OR
     NOT GRANIT_TINT_EXECUTABLE)
    message(FATAL_ERROR "从 HLSL 生成跨后端资产需要完整 Shader Toolchain 根目录")
  endif()

  set(define_arguments)
  foreach(definition IN LISTS ARG_DEFINES)
    list(APPEND define_arguments --define "${definition}")
  endforeach()
  set(object "${ARG_OUTPUT_DIR}/${ARG_NAME}.grshaderobj")
  add_custom_command(
    OUTPUT "${object}" "${object}.spv" "${object}.wgsl"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_OUTPUT_DIR}"
    COMMAND
      "$<TARGET_FILE:granit_asset_tool>" shader compile --toolchain
      "${GRANIT_SHADER_TOOLCHAIN_ROOT}" --input "${ARG_SOURCE}" --entry "${ARG_ENTRY}"
      --stage "${ARG_STAGE}" --spirv-output "${object}.spv" --wgsl-output "${object}.wgsl"
      ${define_arguments}
    COMMAND
      "$<TARGET_FILE:granit_shader_fixture_tool>" object --spirv "${object}.spv"
      --wgsl "${object}.wgsl" --entry "${ARG_ENTRY}" --stage "${ARG_STAGE}"
      --output "${object}" --source "${ARG_SOURCE}" --dxc "${GRANIT_DXC_EXECUTABLE}"
      --tint "${GRANIT_TINT_EXECUTABLE}" ${define_arguments}
    DEPENDS granit_asset_tool granit_shader_fixture_tool "${ARG_SOURCE}"
    COMMENT "从 HLSL 生成 Shader Object ${ARG_NAME}.grshaderobj"
    VERBATIM
  )
  set(${ARG_OUTPUT_VAR} "${object};${object}.spv;${object}.wgsl" PARENT_SCOPE)
endfunction()

function(granit_add_test_shader_object)
  set(options)
  set(one_value_args NAME SOURCE SPIRV WGSL ENTRY HLSL_ENTRY STAGE OUTPUT_DIR OUTPUT_VAR)
  set(multi_value_args DEFINES)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND GRANIT_TINT_EXECUTABLE)
    set(hlsl_entry "${ARG_ENTRY}")
    if(ARG_HLSL_ENTRY)
      set(hlsl_entry "${ARG_HLSL_ENTRY}")
    endif()
    granit_add_test_shader_object_from_hlsl(
      NAME "${ARG_NAME}"
      SOURCE "${ARG_SOURCE}"
      ENTRY "${hlsl_entry}"
      STAGE "${ARG_STAGE}"
      OUTPUT_DIR "${ARG_OUTPUT_DIR}"
      OUTPUT_VAR output
      DEFINES ${ARG_DEFINES}
    )
  else()
    granit_add_test_shader_object_from_payloads(
      NAME "${ARG_NAME}"
      SPIRV "${ARG_SPIRV}"
      WGSL "${ARG_WGSL}"
      ENTRY "${ARG_ENTRY}"
      STAGE "${ARG_STAGE}"
      OUTPUT_DIR "${ARG_OUTPUT_DIR}"
      OUTPUT_VAR output
    )
  endif()
  set(${ARG_OUTPUT_VAR} "${output}" PARENT_SCOPE)
endfunction()

function(granit_add_hlsl_shader_library)
  set(options ALL)
  set(one_value_args NAME MANIFEST OUTPUT CACHE_DIR TARGET REFERENCE)
  set(multi_value_args SOURCES)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_MANIFEST OR NOT ARG_OUTPUT OR NOT ARG_CACHE_DIR OR NOT ARG_TARGET)
    message(FATAL_ERROR "granit_add_hlsl_shader_library 缺少必要参数")
  endif()
  if(NOT GRANIT_SHADER_TOOLCHAIN_ROOT OR NOT GRANIT_DXC_EXECUTABLE OR
     NOT GRANIT_TINT_EXECUTABLE)
    message(FATAL_ERROR "从 HLSL 源清单构建 Shader Library 需要完整 Shader Toolchain 根目录")
  endif()

  get_filename_component(output_directory "${ARG_OUTPUT}" DIRECTORY)
  set(commands
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_CACHE_DIR}" "${output_directory}"
      COMMAND
        "$<TARGET_FILE:granit_asset_tool>" shader build-library --manifest "${ARG_MANIFEST}"
        --toolchain "${GRANIT_SHADER_TOOLCHAIN_ROOT}"
        --cache "${ARG_CACHE_DIR}" --output "${ARG_OUTPUT}")
  set(dependencies granit_asset_tool "${ARG_MANIFEST}" ${ARG_SOURCES})
  if(ARG_REFERENCE)
    list(APPEND commands COMMAND "${CMAKE_COMMAND}" -E compare_files "${ARG_OUTPUT}"
                                 "${ARG_REFERENCE}")
    list(APPEND dependencies "${ARG_REFERENCE}")
  endif()
  set(stamp "${ARG_OUTPUT}.verified")
  list(APPEND commands COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}")
  add_custom_command(
    OUTPUT "${stamp}"
    BYPRODUCTS "${ARG_OUTPUT}"
    ${commands}
    DEPENDS ${dependencies}
    COMMENT "从 HLSL 源清单构建 Shader Library ${ARG_NAME}"
    VERBATIM
  )
  if(ARG_ALL)
    add_custom_target(${ARG_TARGET} ALL DEPENDS "${stamp}")
  else()
    add_custom_target(${ARG_TARGET} DEPENDS "${stamp}")
  endif()
endfunction()

function(granit_prepare_runtime_shader_libraries)
  set(output_root "${CMAKE_BINARY_DIR}/generated/runtime-libraries")
  set(object_root "${CMAKE_BINARY_DIR}/generated/runtime-shader-objects")
  if(GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND GRANIT_TINT_EXECUTABLE)
    granit_add_hlsl_shader_library(
      ALL
      NAME pbr_standard
      MANIFEST "${granit_asset_sources_dir}/shaders/pbr/pbr_standard.grshlib.json"
      OUTPUT "${output_root}/pbr_standard.grshlib"
      CACHE_DIR "${object_root}/pbr-standard"
      REFERENCE "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
      TARGET granit_pbr_shader_library
      SOURCES "${granit_asset_sources_dir}/shaders/pbr/pbr_standard.hlsl"
    )
    granit_add_hlsl_shader_library(
      ALL
      NAME canvas
      MANIFEST "${granit_asset_sources_dir}/shaders/unlit/canvas.grshlib.json"
      OUTPUT "${output_root}/unlit_canvas.grshlib"
      CACHE_DIR "${object_root}/unlit-canvas"
      REFERENCE "${granit_embedded_asset_snapshot_dir}/pipeline/unlit_canvas.grshlib"
      TARGET granit_canvas_shader_library
      SOURCES "${granit_asset_sources_dir}/shaders/unlit/unlit.hlsl"
    )
  endif()
endfunction()

function(granit_prepare_test_shader_assets)
  set(root "${CMAKE_BINARY_DIR}/generated/test-shaders")
  set(pbr_output "${root}/pbr")
  set(pipeline_output "${root}/pipeline")
  set(smoke_output "${root}/smoke")
  set(unlit_output "${root}/unlit")
  set(outputs)

  set(pbr_source "${granit_asset_sources_dir}/shaders/pbr/pbr_untextured.hlsl")
  set(pbr_vertex_names pbr_lights.vert pbr_shadow_ibl_lights.vert pbr_untextured.vert)
  foreach(name IN LISTS pbr_vertex_names)
    set(definitions)
    if(name STREQUAL "pbr_lights.vert")
      set(definitions GRANIT_PBR_LIGHTS=1)
    elseif(name STREQUAL "pbr_shadow_ibl_lights.vert")
      set(
        definitions
        GRANIT_PBR_SHADOWS=1
        GRANIT_PBR_IBL=1
        GRANIT_PBR_LIGHTS=1
      )
    endif()
    granit_add_test_shader_object(
      NAME "${name}"
      SOURCE "${pbr_source}"
      SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/${name}.wgsl"
      ENTRY vertex_main
      STAGE vertex
      OUTPUT_DIR "${pbr_output}"
      OUTPUT_VAR output
      DEFINES ${definitions}
    )
    list(APPEND outputs ${output})
  endforeach()

  foreach(name IN ITEMS unlit.vert unlit.frag unlit_alpha_cutoff.frag)
    if(name MATCHES "\\.vert$")
      set(shader_stage vertex)
      set(entry vertex_main)
    else()
      set(shader_stage fragment)
      set(entry fragment_main)
    endif()
    set(definitions)
    if(name STREQUAL "unlit_alpha_cutoff.frag")
      set(definitions GRANIT_UNLIT_ALPHA_CUTOFF=1)
    elseif(name STREQUAL "unlit.frag")
      set(definitions GRANIT_UNLIT_ALPHA_CUTOFF=0)
    endif()
    granit_add_test_shader_object(
      NAME "${name}"
      SOURCE "${granit_asset_sources_dir}/shaders/unlit/unlit.hlsl"
      SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/unlit/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/unlit/${name}.wgsl"
      ENTRY "${entry}"
      STAGE "${shader_stage}"
      OUTPUT_DIR "${unlit_output}"
      OUTPUT_VAR output
      DEFINES ${definitions}
    )
    list(APPEND outputs ${output})
  endforeach()
  set(
    pbr_fragment_names
    pbr_lights_untextured.frag
    pbr_ibl_lights_untextured.frag
    pbr_shadow_lights_untextured.frag
    pbr_shadow_ibl_lights_untextured.frag
    pbr_untextured.frag
    pbr_textured.frag
  )
  foreach(name IN LISTS pbr_fragment_names)
    set(definitions)
    if(name STREQUAL "pbr_lights_untextured.frag")
      set(definitions GRANIT_PBR_LIGHTS=1)
    elseif(name STREQUAL "pbr_ibl_lights_untextured.frag")
      set(definitions GRANIT_PBR_IBL=1 GRANIT_PBR_LIGHTS=1)
    elseif(name STREQUAL "pbr_shadow_lights_untextured.frag")
      set(definitions GRANIT_PBR_SHADOWS=1 GRANIT_PBR_LIGHTS=1)
    elseif(name STREQUAL "pbr_shadow_ibl_lights_untextured.frag")
      set(
        definitions
        GRANIT_PBR_SHADOWS=1
        GRANIT_PBR_IBL=1
        GRANIT_PBR_LIGHTS=1
        GRANIT_PBR_TEXTURE_MASK=0
      )
    elseif(name STREQUAL "pbr_untextured.frag")
      set(definitions GRANIT_PBR_TEXTURE_MASK=0)
    elseif(name STREQUAL "pbr_textured.frag")
      set(definitions GRANIT_PBR_TEXTURE_MASK=31)
    endif()
    granit_add_test_shader_object(
      NAME "${name}"
      SOURCE "${pbr_source}"
      SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/${name}.wgsl"
      ENTRY fragment_main
      STAGE fragment
      OUTPUT_DIR "${pbr_output}"
      OUTPUT_VAR output
      DEFINES ${definitions}
    )
    list(APPEND outputs ${output})
  endforeach()

  foreach(stage IN ITEMS vert frag)
    if(stage STREQUAL "vert")
      set(shader_stage vertex)
      set(entry vertex_main)
    else()
      set(shader_stage fragment)
      set(entry fragment_main)
    endif()
    granit_add_test_shader_object(
      NAME "tone_mapping.${stage}"
      SOURCE "${granit_asset_sources_dir}/shaders/pipeline/tone_mapping.hlsl"
      SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pipeline/tone_mapping.${stage}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pipeline/tone_mapping.wgsl"
      ENTRY "${entry}"
      STAGE "${shader_stage}"
      OUTPUT_DIR "${pipeline_output}"
      OUTPUT_VAR output
    )
    list(APPEND outputs ${output})
  endforeach()

  set(smoke_vertex_names triangle.vert)
  set(smoke_vertex_sources triangle.hlsl)
  foreach(name source IN ZIP_LISTS smoke_vertex_names smoke_vertex_sources)
    granit_add_test_shader_object(
      NAME "${name}"
      SOURCE "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${source}"
      SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${name}.wgsl"
      ENTRY main
      HLSL_ENTRY vertex_main
      STAGE vertex
      OUTPUT_DIR "${smoke_output}"
      OUTPUT_VAR output
    )
    list(APPEND outputs ${output})
  endforeach()
  set(smoke_fragment_names triangle.frag)
  set(smoke_fragment_sources triangle.hlsl)
  foreach(name source IN ZIP_LISTS smoke_fragment_names smoke_fragment_sources)
    granit_add_test_shader_object(
      NAME "${name}"
      SOURCE "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${source}"
      SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${name}.wgsl"
      ENTRY main
      HLSL_ENTRY fragment_main
      STAGE fragment
      OUTPUT_DIR "${smoke_output}"
      OUTPUT_VAR output
    )
    list(APPEND outputs ${output})
  endforeach()
  granit_add_test_shader_object(
    NAME compute.comp
    SOURCE "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/compute.hlsl"
    SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/compute.comp.spv"
    WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/compute.comp.wgsl"
    ENTRY main
    HLSL_ENTRY compute_main
    STAGE compute
    OUTPUT_DIR "${smoke_output}"
    OUTPUT_VAR output
  )
  list(APPEND outputs ${output})

  set(test_library_root "${CMAKE_BINARY_DIR}/generated/test-libraries")
  set(pbr_test_library "${test_library_root}/pbr.grshlib")
  set(unlit_test_library "${test_library_root}/unlit.grshlib")
  add_custom_command(
    OUTPUT "${pbr_test_library}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${test_library_root}"
    COMMAND
      "$<TARGET_FILE:granit_shader_fixture_tool>" library --name pbr
      --object
      "pbr.vertex/shadow_ibl_lights=${pbr_output}/pbr_shadow_ibl_lights.vert.grshaderobj"
      --object
      "pbr.fragment/untextured=${pbr_output}/pbr_shadow_ibl_lights_untextured.frag.grshaderobj"
      --target all --output "${pbr_test_library}"
    DEPENDS
      granit_shader_fixture_tool
      "${pbr_output}/pbr_shadow_ibl_lights.vert.grshaderobj"
      "${pbr_output}/pbr_shadow_ibl_lights_untextured.frag.grshaderobj"
    COMMENT "从测试 Shader Object 链接 PBR Shader Library"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${unlit_test_library}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${test_library_root}"
    COMMAND
      "$<TARGET_FILE:granit_shader_fixture_tool>" library --name unlit
      --object "unlit.vertex=${unlit_output}/unlit.vert.grshaderobj"
      --object "unlit.fragment/opaque=${unlit_output}/unlit.frag.grshaderobj"
      --object
      "unlit.fragment/alpha_cutoff=${unlit_output}/unlit_alpha_cutoff.frag.grshaderobj"
      --target all --output "${unlit_test_library}"
    DEPENDS
      granit_shader_fixture_tool
      "${unlit_output}/unlit.vert.grshaderobj"
      "${unlit_output}/unlit.frag.grshaderobj"
      "${unlit_output}/unlit_alpha_cutoff.frag.grshaderobj"
    COMMENT "从测试 Shader Object 链接 Unlit Shader Library"
    VERBATIM
  )
  add_custom_target(
    granit_test_shader_assets DEPENDS ${outputs} "${pbr_test_library}" "${unlit_test_library}"
  )
  set(GRANIT_PBR_TEST_SHADER_DIR "${pbr_output}" PARENT_SCOPE)
  set(GRANIT_PIPELINE_TEST_SHADER_DIR "${pipeline_output}" PARENT_SCOPE)
  set(GRANIT_SMOKE_TEST_SHADER_DIR "${smoke_output}" PARENT_SCOPE)
  set(GRANIT_UNLIT_TEST_SHADER_DIR "${unlit_output}" PARENT_SCOPE)
  set(GRANIT_PBR_TEST_SHADER_LIBRARY "${pbr_test_library}" PARENT_SCOPE)
  set(GRANIT_UNLIT_TEST_SHADER_LIBRARY "${unlit_test_library}" PARENT_SCOPE)
endfunction()
