include_guard(GLOBAL)

add_library(pedantic INTERFACE)
if(NOT MSVC)
  target_compile_options(pedantic INTERFACE "-Wall" "-Werror" "-pedantic"
                                            "-Wshadow")
else()
  target_compile_options(pedantic INTERFACE "/W4" "/WX" "/permissive-")
endif()

add_library(pedantic::pedantic ALIAS pedantic)
