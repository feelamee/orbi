set(CMAKE_CXX_STANDARD_REQUIRED
    ON
    CACHE PATH "")

set(CMAKE_CXX_EXTENSIONS
    OFF
    CACHE PATH "")

set(ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.."
    CACHE PATH "")

message(STATUS "ROOT: ${ROOT}")

set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH};${ROOT}/cmake/"
    CACHE PATH "")

message(STATUS "CMAKE_MODULES_PATH: ${CMAKE_MODULE_PATH}")

set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE PATH "")
