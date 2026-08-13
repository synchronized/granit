# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_consumer LANGUAGES C CXX)

find_package(granit CONFIG REQUIRED)

add_executable(granit_c_consumer main.c)
target_link_libraries(granit_c_consumer PRIVATE granit::granit)
set_target_properties(granit_c_consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)

add_executable(granit_cpp_consumer main.cpp)
target_link_libraries(granit_cpp_consumer PRIVATE granit::granit)
target_compile_features(granit_cpp_consumer PRIVATE cxx_std_20)
