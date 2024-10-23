set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(CMAKE_C_COMPILER NAMES clang)
find_program(CMAKE_CXX_COMPILER NAMES clang++)

if(NOT CMAKE_C_COMPILER)
  message(FATAL_ERROR "Failed to find CMAKE_C_COMPILER.")
endif()

if(NOT CMAKE_CXX_COMPILER)
  message(FATAL_ERROR "Failed to find CMAKE_CXX_COMPILER.")
endif()
