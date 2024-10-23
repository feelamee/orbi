cmake_minimum_required(VERSION 3.5)
project(orbi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(include_dir "${CMAKE_CURRENT_SOURCE_DIR}/include/")

if(BULID_SHARED_LIBS)
  add_library(orbi SHARED)
else()
  add_library(orbi STATIC)
endif()

target_sources(orbi PRIVATE "${include_dir}/orbi/util.hpp" "src/util.cpp")
target_compile_features(orbi PUBLIC cxx_std_20)
target_include_directories(orbi PUBLIC "${include_dir}")

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
  INCLUDE_GUARD_NAME "ORIB_${export_file_name_header_guard}")

target_compile_definitions(
  orbi PRIVATE "$<$<BOOL:${BUILD_SHARED_LIBS}>:ORBI_BUILD_TYPE_SHARED>")

add_library(orbi::orbi ALIAS orbi)
