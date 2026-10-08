# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_RENDERDOC_TEST OR NOT DEFINED GRANIT_GRANIT_DIR OR
   NOT DEFINED GRANIT_SUBSTITUTE OR NOT DEFINED GRANIT_OUTPUT_DIR OR
   NOT DEFINED GRANIT_TRACE_VALIDATOR OR NOT DEFINED GRANIT_TRACE_VALIDATOR_BINARY)
  message(FATAL_ERROR "RenderDoc bridge test requires renderer, Granit, substitute and output paths")
endif()

file(MAKE_DIRECTORY "${GRANIT_OUTPUT_DIR}")
set(marker "${GRANIT_OUTPUT_DIR}/renderdoc-substitute.marker")
set(trace "${GRANIT_OUTPUT_DIR}/renderdoc-substitute-trace.jsonl")
file(REMOVE "${marker}" "${trace}")

if(WIN32)
  set(path_separator ";")
else()
  set(path_separator ":")
endif()
set(runtime_path "${GRANIT_GRANIT_DIR}${path_separator}$ENV{PATH}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "GRANIT_RENDERDOC=trigger"
          "GRANIT_RENDERDOC_PATH=${GRANIT_SUBSTITUTE}"
          "GRANIT_RENDERDOC_SUBSTITUTE_MARKER=${marker}"
          "GRANIT_FRAME_TRACE=${trace}"
          "PATH=${runtime_path}"
          "${GRANIT_RENDERDOC_TEST}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Renderer failed with RenderDoc substitute (${result})\n${output}\n${error}")
endif()
if(NOT EXISTS "${marker}")
  message(FATAL_ERROR "RenderDoc substitute did not receive TriggerCapture")
endif()
file(READ "${marker}" marker_contents)
if(NOT marker_contents MATCHES "triggered")
  message(FATAL_ERROR "Unexpected RenderDoc substitute marker: ${marker_contents}")
endif()
if(NOT EXISTS "${trace}")
  message(FATAL_ERROR "RenderDoc substitute run did not produce Frame Trace")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DGRANIT_TRACE=${trace}" -P "${GRANIT_TRACE_VALIDATOR}"
  RESULT_VARIABLE baseline_result
  OUTPUT_VARIABLE baseline_output
  ERROR_VARIABLE baseline_error
)
if(NOT baseline_result EQUAL 0)
  message(FATAL_ERROR "Frame Trace baseline validation failed\n${baseline_output}\n${baseline_error}")
endif()

execute_process(
  COMMAND "${GRANIT_TRACE_VALIDATOR_BINARY}" "${trace}"
  RESULT_VARIABLE trace_result
  OUTPUT_VARIABLE trace_output
  ERROR_VARIABLE trace_error
)
if(NOT trace_result EQUAL 0)
  message(FATAL_ERROR "Frame Trace validation failed\n${trace_output}\n${trace_error}")
endif()

if(WIN32)
  execute_process(
    COMMAND powershell.exe -NoProfile -NonInteractive -Command
            "$records = Get-Content -LiteralPath '${trace}' | ForEach-Object { $_ | ConvertFrom-Json }; if ($records.Count -eq 0) { exit 1 }; foreach ($record in $records) { if ($null -eq $record.sequence -or $null -eq $record.timestamp_ns -or [string]::IsNullOrEmpty($record.kind)) { exit 2 } }"
    RESULT_VARIABLE json_result
  )
  if(NOT json_result EQUAL 0)
    message(FATAL_ERROR "Windows JSON parser rejected RenderDoc bridge Trace")
  endif()
endif()

message(STATUS "RenderDoc bridge substitute capture verified")
