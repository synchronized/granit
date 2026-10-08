# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_TRACE_VALIDATOR_BINARY OR NOT DEFINED GRANIT_OUTPUT_DIR)
  message(FATAL_ERROR "Frame Trace validator contract requires binary and output paths")
endif()

file(REMOVE_RECURSE "${GRANIT_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${GRANIT_OUTPUT_DIR}")

set(valid_trace "${GRANIT_OUTPUT_DIR}/valid.jsonl")
file(
  WRITE "${valid_trace}"
  [=[{"schema_version":1,"sequence":1,"timestamp_ns":10,"kind":"frame"}
{"schema_version":1,"sequence":2,"timestamp_ns":20,"kind":"trace_summary","dropped_events":0,"flush_failures":0}
]=]
)

execute_process(
  COMMAND "${GRANIT_TRACE_VALIDATOR_BINARY}" "${valid_trace}"
  RESULT_VARIABLE valid_result
)
if(NOT valid_result EQUAL 0)
  message(FATAL_ERROR "Valid Frame Trace was rejected")
endif()

function(expect_rejected name content)
  set(trace "${GRANIT_OUTPUT_DIR}/${name}.jsonl")
  file(WRITE "${trace}" "${content}")
  execute_process(
    COMMAND "${GRANIT_TRACE_VALIDATOR_BINARY}" "${trace}"
    RESULT_VARIABLE result
  )
  if(result EQUAL 0)
    message(FATAL_ERROR "Invalid Frame Trace was accepted: ${name}")
  endif()
endfunction()

expect_rejected(
  truncated
  [=[{"schema_version":1,"sequence":1,"timestamp_ns":10,"kind":"frame"
]=]
)
expect_rejected(
  unknown_schema
  [=[{"schema_version":2,"sequence":1,"timestamp_ns":10,"kind":"frame"}
]=]
)
expect_rejected(
  out_of_order
  [=[{"schema_version":1,"sequence":2,"timestamp_ns":20,"kind":"frame"}
{"schema_version":1,"sequence":1,"timestamp_ns":30,"kind":"trace_summary","dropped_events":0,"flush_failures":0}
]=]
)

file(REMOVE_RECURSE "${GRANIT_OUTPUT_DIR}")
