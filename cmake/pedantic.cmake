include_guard(GLOBAL)

add_library(pedantic INTERFACE)
target_compile_options(pedantic INTERFACE "-Wall" "-Werror" "-pedantic"
                                          "-Wshadow")

add_library(pedantic::pedantic ALIAS pedantic)
