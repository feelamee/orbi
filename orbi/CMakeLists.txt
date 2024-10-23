cmake_minimum_required(VERSION 3.5)
project(orbi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/")

add_library(orbi STATIC)
target_sources(orbi PRIVATE "${INCLUDE_DIR}/orbi/util.hpp" "src/util.cpp")
target_compile_features(orbi PUBLIC cxx_std_20)
target_include_directories(orbi PUBLIC "${INCLUDE_DIR}")

add_library(orbi::orbi ALIAS orbi)
