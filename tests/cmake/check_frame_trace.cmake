# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_TRACE)
  message(FATAL_ERROR "GRANIT_TRACE is required")
endif()
if(NOT EXISTS "${GRANIT_TRACE}")
  message(FATAL_ERROR "Frame Trace does not exist: ${GRANIT_TRACE}")
endif()

file(STRINGS "${GRANIT_TRACE}" lines)
list(LENGTH lines line_count)
if(line_count EQUAL 0)
  message(FATAL_ERROR "Frame Trace is empty: ${GRANIT_TRACE}")
endif()

file(SIZE "${GRANIT_TRACE}" byte_count)
if(byte_count LESS 2)
  message(FATAL_ERROR "Frame Trace is unexpectedly small: ${GRANIT_TRACE}")
endif()

message(STATUS "Validated non-empty Frame Trace JSONL with ${line_count} records")
