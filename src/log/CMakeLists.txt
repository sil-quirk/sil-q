cmake_minimum_required(VERSION 3.18)
project(log LANGUAGES C)

add_library(log OBJECT)
target_compile_options(log PRIVATE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
  $<$<C_COMPILER_ID:MSVC>:/W4>
)
target_compile_definitions(log PRIVATE LOG_USE_COLOR)
set_target_properties(log PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_sources(log PRIVATE log.c)
