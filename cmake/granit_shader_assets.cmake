# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

function(granit_add_shader_object)
  set(options)
  set(one_value_args NAME SPIRV WGSL ENTRY STAGE OUTPUT_DIR OUTPUT_VAR)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SPIRV OR NOT ARG_WGSL OR NOT ARG_ENTRY OR NOT ARG_STAGE OR
     NOT ARG_OUTPUT_DIR)
    message(FATAL_ERROR "granit_add_shader_object 缺少必要参数")
  endif()

  set(object "${ARG_OUTPUT_DIR}/${ARG_NAME}.grshaderobj")
  add_custom_command(
    OUTPUT "${object}" "${object}.spv" "${object}.wgsl"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_OUTPUT_DIR}"
    COMMAND
      "$<TARGET_FILE:granit_shader_tool>" object --spirv "${ARG_SPIRV}" --wgsl "${ARG_WGSL}"
      --entry "${ARG_ENTRY}" --stage "${ARG_STAGE}" --output "${object}"
    DEPENDS granit_shader_tool "${ARG_SPIRV}" "${ARG_WGSL}"
    COMMENT "生成 Shader Object ${ARG_NAME}.grshaderobj"
    VERBATIM
  )
  set(${ARG_OUTPUT_VAR} "${object};${object}.spv;${object}.wgsl" PARENT_SCOPE)
endfunction()

function(granit_add_hlsl_shader_object)
  set(options)
  set(one_value_args NAME SOURCE ENTRY STAGE OUTPUT_DIR OUTPUT_VAR)
  set(multi_value_args DEFINES)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SOURCE OR NOT ARG_ENTRY OR NOT ARG_STAGE OR NOT ARG_OUTPUT_DIR)
    message(FATAL_ERROR "granit_add_hlsl_shader_object 缺少必要参数")
  endif()
  if(NOT GRANIT_DXC_EXECUTABLE OR NOT GRANIT_TINT_EXECUTABLE)
    message(FATAL_ERROR "从 HLSL 生成跨后端资产需要 DXC 和 Tint")
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
      "$<TARGET_FILE:granit_shader_tool>" compile-hlsl --dxc "${GRANIT_DXC_EXECUTABLE}"
      --tint "${GRANIT_TINT_EXECUTABLE}" --input "${ARG_SOURCE}" --entry "${ARG_ENTRY}"
      --stage "${ARG_STAGE}" --spirv-output "${object}.spv" --wgsl-output "${object}.wgsl"
      --object "${object}" --object-backend all ${define_arguments}
    DEPENDS granit_shader_tool "${ARG_SOURCE}"
    COMMENT "从 HLSL 生成 Shader Object ${ARG_NAME}.grshaderobj"
    VERBATIM
  )
  set(${ARG_OUTPUT_VAR} "${object};${object}.spv;${object}.wgsl" PARENT_SCOPE)
endfunction()

function(granit_add_shader_library)
  set(options ALL)
  set(one_value_args NAME OUTPUT REFERENCE TARGET)
  set(multi_value_args OBJECTS)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_OUTPUT OR NOT ARG_OBJECTS OR NOT ARG_TARGET)
    message(FATAL_ERROR "granit_add_shader_library 缺少必要参数")
  endif()

  set(object_arguments)
  set(dependencies granit_shader_tool)
  foreach(object IN LISTS ARG_OBJECTS)
    list(APPEND object_arguments --object "${object}")
    list(APPEND dependencies "${object}" "${object}.spv" "${object}.wgsl")
  endforeach()
  get_filename_component(output_directory "${ARG_OUTPUT}" DIRECTORY)
  set(stamp "${ARG_OUTPUT}.verified")
  set(commands
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_directory}"
      COMMAND "$<TARGET_FILE:granit_shader_tool>" library ${object_arguments}
              --target all --output "${ARG_OUTPUT}")
  if(ARG_REFERENCE)
    list(APPEND commands
         COMMAND "${CMAKE_COMMAND}" -E compare_files "${ARG_OUTPUT}" "${ARG_REFERENCE}")
    list(APPEND dependencies "${ARG_REFERENCE}")
  endif()
  list(APPEND commands COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}")
  add_custom_command(
    OUTPUT "${stamp}"
    BYPRODUCTS "${ARG_OUTPUT}"
    ${commands}
    DEPENDS ${dependencies}
    COMMENT "链接 Shader Library ${ARG_NAME}"
    COMMAND_EXPAND_LISTS
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
  granit_add_shader_object(
    NAME pbr_standard.vert
    SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.vert.spv"
    WGSL "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.vert.wgsl"
    ENTRY vertex_main
    STAGE vertex
    OUTPUT_DIR "${object_root}/pbr"
    OUTPUT_VAR pbr_vertex_outputs
  )
  granit_add_shader_object(
    NAME pbr_standard.frag
    SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.frag.spv"
    WGSL "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.frag.wgsl"
    ENTRY fragment_main
    STAGE fragment
    OUTPUT_DIR "${object_root}/pbr"
    OUTPUT_VAR pbr_fragment_outputs
  )
  list(GET pbr_vertex_outputs 0 pbr_vertex_object)
  list(GET pbr_fragment_outputs 0 pbr_fragment_object)
  granit_add_shader_library(
    ALL
    NAME pbr_standard
    OUTPUT "${output_root}/pbr_standard.grshlib"
    REFERENCE "${PROJECT_SOURCE_DIR}/assets/libraries/pbr_standard.grshlib"
    TARGET granit_pbr_shader_library
    OBJECTS "${pbr_vertex_object}" "${pbr_fragment_object}"
  )
  set(canvas_objects)
  foreach(name IN ITEMS unlit_canvas.vert unlit_canvas.frag unlit_canvas_encode_srgb.frag)
    if(name MATCHES "\\.vert$")
      set(stage vertex)
      set(entry canvas_vertex_main)
    else()
      set(stage fragment)
      set(entry fragment_main)
    endif()
    granit_add_shader_object(
      NAME "${name}"
      SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/${name}.wgsl"
      ENTRY "${entry}"
      STAGE "${stage}"
      OUTPUT_DIR "${object_root}/unlit"
      OUTPUT_VAR object_outputs
    )
    list(GET object_outputs 0 object)
    list(APPEND canvas_objects "${object}")
  endforeach()
  granit_add_shader_library(
    ALL
    NAME unlit_canvas
    OUTPUT "${output_root}/unlit_canvas.grshlib"
    REFERENCE "${PROJECT_SOURCE_DIR}/src/pipeline/assets/unlit_canvas.grshlib"
    TARGET granit_canvas_shader_library
    OBJECTS ${canvas_objects}
  )
endfunction()

function(granit_prepare_test_shader_assets)
  set(root "${CMAKE_BINARY_DIR}/generated/test-shaders")
  set(pbr_output "${root}/pbr")
  set(pipeline_output "${root}/pipeline")
  set(smoke_output "${root}/smoke")
  set(unlit_output "${root}/unlit")
  set(outputs)

  set(pbr_vertex_names pbr_lights.vert pbr_shadow_ibl_lights.vert pbr_untextured.vert)
  foreach(name IN LISTS pbr_vertex_names)
    granit_add_shader_object(
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

  foreach(name IN ITEMS unlit.vert unlit.frag unlit_alpha_cutoff.frag)
    if(name MATCHES "\\.vert$")
      set(shader_stage vertex)
      set(entry vertex_main)
    else()
      set(shader_stage fragment)
      set(entry fragment_main)
    endif()
    granit_add_shader_object(
      NAME "${name}"
      SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/${name}.spv"
      WGSL "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/${name}.wgsl"
      ENTRY "${entry}"
      STAGE "${shader_stage}"
      OUTPUT_DIR "${unlit_output}"
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
    pbr_untextured.frag
    pbr_textured.frag
  )
  foreach(name IN LISTS pbr_fragment_names)
    granit_add_shader_object(
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
    granit_add_shader_object(
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
    granit_add_shader_object(
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
    granit_add_shader_object(
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
  granit_add_shader_object(
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
  set(GRANIT_UNLIT_TEST_SHADER_DIR "${unlit_output}" PARENT_SCOPE)
endfunction()
