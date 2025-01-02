cmake_minimum_required(VERSION 3.5)
project(example-cube)

add_executable(cube "src/main.cpp")
target_compile_features(cube PRIVATE cxx_std_20)

include(pedantic)
include(sanitizer)

target_link_libraries(cube
                      PRIVATE $<$<BOOL:${ORBI_PEDANTIC}>:pedantic::pedantic>)
target_link_libraries(
  cube PRIVATE $<$<BOOL:${ORBI_SANITIZER}>:sanitizer::address
                   sanitizer::undefined>)

# TODO: let's think up something better... e.g. using cmake modules
add_subdirectory("${ROOT}/orbi" "${CMAKE_CURRENT_BINARY_DIR}/orbi")
target_link_libraries(cube PRIVATE orbi::orbi)
