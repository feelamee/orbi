cmake_minimum_required(VERSION 3.5)
project(triangle)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

list(APPEND CMAKE_MODULE_PATH "${ROOT}/cmake/")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(triangle "src/main.cpp")
target_compile_features(triangle PRIVATE cxx_std_20)

include(pedantic)
include(sanitizer)

target_link_libraries(triangle
                      PRIVATE $<$<BOOL:${ORBI_PEDANTIC}>:pedantic::pedantic>)
target_link_libraries(
  triangle PRIVATE $<$<BOOL:${ORBI_SANITIZER}>:sanitizer::address
                   sanitizer::undefined>)

# TODO: let's think up something better... e.g. using cmake modules
add_subdirectory("${ROOT}/orbi" "${CMAKE_CURRENT_BINARY_DIR}/orbi")
target_link_libraries(triangle PRIVATE orbi::orbi)
