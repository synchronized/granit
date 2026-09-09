# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

function(granit_add_packed_shader_asset)
  set(options)
  set(one_value_args NAME SPIRV WGSL ENTRY STAGE OUTPUT_DIR OUTPUT_VAR)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SPIRV OR NOT ARG_WGSL OR NOT ARG_ENTRY OR NOT ARG_STAGE OR
     NOT ARG_OUTPUT_DIR)
    message(FATAL_ERROR "granit_add_packed_shader_asset 缺少必要参数")
  endif()

  set(asset "${ARG_OUTPUT_DIR}/${ARG_NAME}.grshader")
  add_custom_command(
    OUTPUT "${asset}" "${asset}.spv" "${asset}.wgsl"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_OUTPUT_DIR}"
    COMMAND
      "$<TARGET_FILE:granit_shader_tool>" pack --spirv "${ARG_SPIRV}" --wgsl "${ARG_WGSL}"
      --entry "${ARG_ENTRY}" --stage "${ARG_STAGE}" --asset "${asset}"
    DEPENDS granit_shader_tool "${ARG_SPIRV}" "${ARG_WGSL}"
    COMMENT "生成 Shader 资产 ${ARG_NAME}.grshader"
    VERBATIM
  )
  set(${ARG_OUTPUT_VAR} "${asset};${asset}.spv;${asset}.wgsl" PARENT_SCOPE)
endfunction()

function(granit_add_hlsl_shader_asset)
  set(options)
  set(one_value_args NAME SOURCE ENTRY STAGE OUTPUT_DIR OUTPUT_VAR)
  set(multi_value_args DEFINES)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SOURCE OR NOT ARG_ENTRY OR NOT ARG_STAGE OR NOT ARG_OUTPUT_DIR)
    message(FATAL_ERROR "granit_add_hlsl_shader_asset 缺少必要参数")
  endif()
  if(NOT GRANIT_DXC_EXECUTABLE OR NOT GRANIT_TINT_EXECUTABLE)
    message(FATAL_ERROR "从 HLSL 生成跨后端资产需要 DXC 和 Tint")
  endif()

  set(define_arguments)
  foreach(definition IN LISTS ARG_DEFINES)
    list(APPEND define_arguments --define "${definition}")
  endforeach()
  set(asset "${ARG_OUTPUT_DIR}/${ARG_NAME}.grshader")
  add_custom_command(
    OUTPUT "${asset}" "${asset}.spv" "${asset}.wgsl"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_OUTPUT_DIR}"
    COMMAND
      "$<TARGET_FILE:granit_shader_tool>" compile-hlsl --dxc "${GRANIT_DXC_EXECUTABLE}"
      --tint "${GRANIT_TINT_EXECUTABLE}" --input "${ARG_SOURCE}" --entry "${ARG_ENTRY}"
      --stage "${ARG_STAGE}" --spirv-output "${asset}.spv" --wgsl-output "${asset}.wgsl"
      --asset "${asset}" --asset-backend all ${define_arguments}
    DEPENDS granit_shader_tool "${ARG_SOURCE}"
    COMMENT "从 HLSL 生成 Shader 资产 ${ARG_NAME}.grshader"
    VERBATIM
  )
  set(${ARG_OUTPUT_VAR} "${asset};${asset}.spv;${asset}.wgsl" PARENT_SCOPE)
endfunction()

function(granit_prepare_test_shader_assets)
  set(root "${CMAKE_BINARY_DIR}/generated/test-shaders")
  set(pbr_output "${root}/pbr")
  set(pipeline_output "${root}/pipeline")
  set(smoke_output "${root}/smoke")
  set(outputs)

  set(pbr_vertex_names pbr_lights.vert pbr_shadow_ibl_lights.vert)
  foreach(name IN LISTS pbr_vertex_names)
    granit_add_packed_shader_asset(
      NAME "${name}"
      SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/${name}.wgsl"
      ENTRY vertex_main
      STAGE vertex
      OUTPUT_DIR "${pbr_output}"
      OUTPUT_VAR output
    )
    list(APPEND outputs ${output})
  endforeach()
  set(
    pbr_fragment_names
    pbr_lights_untextured.frag
    pbr_ibl_lights_untextured.frag
    pbr_shadow_lights_untextured.frag
    pbr_shadow_ibl_lights_untextured.frag
  )
  foreach(name IN LISTS pbr_fragment_names)
    granit_add_packed_shader_asset(
      NAME "${name}"
      SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/${name}.wgsl"
      ENTRY fragment_main
      STAGE fragment
      OUTPUT_DIR "${pbr_output}"
      OUTPUT_VAR output
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
    granit_add_packed_shader_asset(
      NAME "tone_mapping.${stage}"
      SPIRV "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.${stage}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.wgsl"
      ENTRY "${entry}"
      STAGE "${shader_stage}"
      OUTPUT_DIR "${pipeline_output}"
      OUTPUT_VAR output
    )
    list(APPEND outputs ${output})
  endforeach()

  set(smoke_vertex_names triangle.vert window_triangle.vert)
  foreach(name IN LISTS smoke_vertex_names)
    granit_add_packed_shader_asset(
      NAME "${name}"
      SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${name}.wgsl"
      ENTRY main
      STAGE vertex
      OUTPUT_DIR "${smoke_output}"
      OUTPUT_VAR output
    )
    list(APPEND outputs ${output})
  endforeach()
  set(smoke_fragment_names triangle.frag window_triangle.frag)
  foreach(name IN LISTS smoke_fragment_names)
    granit_add_packed_shader_asset(
      NAME "${name}"
      SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/${name}.wgsl"
      ENTRY main
      STAGE fragment
      OUTPUT_DIR "${smoke_output}"
      OUTPUT_VAR output
    )
    list(APPEND outputs ${output})
  endforeach()
  granit_add_packed_shader_asset(
    NAME compute.comp
    SPIRV "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/compute.comp.spv"
    WGSL "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/compute.comp.wgsl"
    ENTRY main
    STAGE compute
    OUTPUT_DIR "${smoke_output}"
    OUTPUT_VAR output
  )
  list(APPEND outputs ${output})

  add_custom_target(granit_test_shader_assets DEPENDS ${outputs})
  set(GRANIT_PBR_TEST_SHADER_DIR "${pbr_output}" PARENT_SCOPE)
  set(GRANIT_PIPELINE_TEST_SHADER_DIR "${pipeline_output}" PARENT_SCOPE)
  set(GRANIT_SMOKE_TEST_SHADER_DIR "${smoke_output}" PARENT_SCOPE)
endfunction()
