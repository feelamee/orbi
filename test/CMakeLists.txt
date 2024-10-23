cmake_minimum_required(VERSION 3.5)
project(test)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY "https://github.com/doctest/doctest"
  GIT_TAG "v2.4.11")
FetchContent_MakeAvailable(doctest)

add_executable(test "main.test.cpp")
target_compile_features(test PRIVATE cxx_std_20)
target_link_libraries(test PRIVATE doctest::doctest)

if(ORBI_PEDANTIC)
  include("../cmake/pedantic.cmake")
  target_link_libraries(test PRIVATE pedantic::pedantic)
endif()

if(ORBI_SANITIZER)
  include("../cmake/sanitizer.cmake")
  target_link_libraries(test PRIVATE sanitizer::address sanitizer::undefined)
endif()

# TODO: let's think up something better...
add_subdirectory("../orbi" "${CMAKE_BINARY_DIR}/orbi")
target_link_libraries(test PRIVATE orbi::orbi)
