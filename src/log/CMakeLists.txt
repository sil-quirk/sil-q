cmake_minimum_required(VERSION 3.18)
project(log LANGUAGES C)

add_library(log OBJECT)
target_compile_options(log PRIVATE -Wall -Wextra -DLOG_USE_COLOR -std=c17)
set_target_properties(log PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_sources(log PRIVATE log.c)
