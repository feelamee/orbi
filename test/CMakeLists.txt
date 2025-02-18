cmake_minimum_required(VERSION 3.5)
project(test)

include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY "https://github.com/doctest/doctest"
  GIT_TAG "v2.4.11")
FetchContent_MakeAvailable(doctest)

add_executable(test "main.test.cpp" "orbi/context.test.cpp" "orbi/window.test.cpp")
target_compile_features(test PRIVATE cxx_std_20)
target_link_libraries(test PRIVATE doctest::doctest)

include(pedantic)
include(sanitizer)

target_link_libraries(test
                      PRIVATE $<$<BOOL:${ORBI_PEDANTIC}>:pedantic::pedantic>)
target_link_libraries(
  test PRIVATE $<$<BOOL:${ORBI_SANITIZER}>:sanitizer::address
               sanitizer::undefined>)

# TODO: let's think up something better... e.g. using cmake modules
add_subdirectory("${ROOT}/orbi" "${CMAKE_CURRENT_BINARY_DIR}/orbi")
target_link_libraries(test PRIVATE orbi::orbi)
