cmake_minimum_required(VERSION 3.18)
project(sil_more_android C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The Sil-More repo root from this file:
# android/app/src/main/cpp -> repo root is five levels up.
get_filename_component(SIL_MORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)

# Build Sil-More (top-level CMake builds a shared library on Android and outputs libmain.so).
add_subdirectory("${SIL_MORE_ROOT}" "${CMAKE_BINARY_DIR}/sil-more")
