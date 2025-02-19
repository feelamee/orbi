cmake_minimum_required(VERSION 3.5)
project(orbi LANGUAGES CXX)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(include_dir "${CMAKE_CURRENT_SOURCE_DIR}/include/")

message(STATUS "BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")

if(BUILD_SHARED_LIBS)
  add_library(orbi SHARED)
else()
  add_library(orbi STATIC)
endif()

target_sources(
  orbi
  PRIVATE "${include_dir}/orbi/detail/util.hpp"
          "${include_dir}/orbi/detail/pimpl.hpp"
          "${include_dir}/orbi/detail/impl.hpp"
          "src/util.cpp"
          "${include_dir}/orbi/context.hpp"
          "src/context.cpp"
          "${include_dir}/orbi/exception.hpp"
          "${include_dir}/orbi/window.hpp"
          "src/window.cpp"
          "${include_dir}/orbi/device.hpp"
          "src/device.cpp")
target_compile_features(orbi PUBLIC cxx_std_20)
target_compile_options(orbi PUBLIC -fmodules-ts)

target_include_directories(orbi PUBLIC "${include_dir}")
target_link_libraries(orbi PRIVATE $<$<BOOL:${ANDROID}>:anroid log>)
target_compile_definitions(orbi PUBLIC VULKAN_HPP_NO_CONSTRUCTORS
                                       VULKAN_HPP_NO_SPACESHIP_OPERATOR
                                       VULKAN_HPP_NO_STRUCT_SETTERS)

include(pedantic)
target_link_libraries(orbi
                      PUBLIC $<$<BOOL:${ORBI_PEDANTIC}>:pedantic::pedantic>)

include(sanitizer)
target_link_libraries(orbi PUBLIC $<$<BOOL:${ORBI_SANITIZER}>:sanitizer::address
                                  sanitizer::undefined>)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

if(BUILD_SHARED_LIBS)
  set(SDL_SHARED ON)
  set(SDL_STATIC OFF)
else()
  set(SDL_SHARED OFF)
  set(SDL_STATIC ON)
endif()

FetchContent_Declare(
  Vulkan-Headers
  GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers"
  GIT_TAG "v1.4.303")
FetchContent_MakeAvailable(Vulkan-Headers)

# TODO: use `PRIVATE` after adding all api needed in examples to `orbi`
find_package(Vulkan REQUIRED)
target_link_libraries(orbi PUBLIC Vulkan::Headers Vulkan::Vulkan)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL"
  GIT_TAG "535d80badefc83c5c527ec5748f2a20d6a9310fe" # release-3.2.0
)
FetchContent_MakeAvailable(SDL3)

# TODO: use `PRIVATE` after adding all api needed in examples to `orbi`
target_link_libraries(
  orbi
  PUBLIC $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,SDL3::SDL3-shared,SDL3::SDL3-static>)

include(GenerateExportHeader)

if(BUILD_SHARED_LIBS)
  set(export_file_name "export-shared.hpp")
else()
  set(export_file_name "export-static.hpp")
endif()

string(MAKE_C_IDENTIFIER ${export_file_name} export_file_name_c_identifier)
string(TOUPPER ${export_file_name_c_identifier} export_file_name_header_guard)

generate_export_header(
  orbi EXPORT_FILE_NAME "${include_dir}/orbi/generated/${export_file_name}"
  INCLUDE_GUARD_NAME "ORBI_${export_file_name_header_guard}")

target_compile_definitions(
  orbi PRIVATE "$<$<BOOL:${BUILD_SHARED_LIBS}>:ORBI_BUILD_TYPE_SHARED>")

add_library(orbi::orbi ALIAS orbi)
