# File: CMakeLists.txt
#
# CMake build configuration for Sil-Q.
#
# Supported platforms and GUI backends:
#   - Windows:       Windows GDI (MinGW/MSYS2)
#   - macOS native:  Cocoa as a Sil.app bundle
#   - macOS console: GCU (curses/ncurses)
#   - Linux:         X11 and/or GCU (curses/ncurses)
#
# Quick start:
#   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Debug build with sanitizers:
#   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_ENABLE_WERROR=ON -DBUILD_ENABLE_SANITIZERS=ON
#   cmake --build build
#
# =============================================================================
# Microsoft Windows (MSYS2/MinGW)
# =============================================================================
#   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# =============================================================================
# macOS native Cocoa app (universal binary for arm64 and x86_64)
# =============================================================================
#   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# =============================================================================
# macOS native Cocoa app (Intel-only, for older Macs)
# =============================================================================
#   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
#   cmake --build build
#
# =============================================================================
# macOS console with GCU (curses/ncurses)
# =============================================================================
#   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DSUPPORT_COCOA_FRONTEND=OFF
#   cmake --build build
#
# =============================================================================
# Linux with X11 and GCU (curses/ncurses)
# =============================================================================
#   cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#

cmake_minimum_required(VERSION 3.16)
message(STATUS "CMake version: ${CMAKE_VERSION}")

# `-DCMAKE_EXPORT_COMPILE_COMMANDS` generates `build/compile_commands.json`,
# which is needed for tools like clang-tidy and the clangd LSP server
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Restrict the available build configurations to only "Debug" and "Release" for
# multi-configuration generators (like Ninja Multi-Config, Xcode, Visual Studio)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# =============================================================================
# Helper functions
# =============================================================================

# Heuristically detect if Clang is being used.
# To give better build error messages on macOS, we need to check the configured
# compiler before CMAKE_C_COMPILER or CMAKE_C_COMPILER_ID are made available by
# `project()`. This is an ok-ish workaround for that.
set(_cc_env_set_to_clang FALSE)
if ("$ENV{CC}" MATCHES "(^|/)clang(-[0-9]+)?$")
    set(_cc_env_set_to_clang TRUE)
endif()

# =============================================================================
# Set build defaults for Windows, macOS, Linux
# =============================================================================

# Platform-specific options with appropriate defaults
if (APPLE)
    option(SUPPORT_COCOA_FRONTEND "Build macOS native Cocoa app" ON)
    option(SUPPORT_GCU_FRONTEND   "Build with GCU (curses/ncurses) support" OFF)
    option(SUPPORT_X11_FRONTEND   "Build with X11 support" OFF)

    # Cocoa: build a universal binary by default
    if (SUPPORT_COCOA_FRONTEND AND NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    endif()

    # non-Cocoa: build only for the host architecture
    if (NOT SUPPORT_COCOA_FRONTEND AND NOT CMAKE_OSX_ARCHITECTURES)
        execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE _host_arch
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(CMAKE_OSX_ARCHITECTURES "${_host_arch}" CACHE STRING "macOS architectures" FORCE)
    endif()

    # macOS deployment target must be set before project() to ensure the
    # compiler is configured with `-mmacosx-version-min`. This prevents
    # deprecation warnings for APIs deprecated after the target version (e.g.,
    # macOS 10.11+).
    if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6" CACHE STRING "Minimum macOS version" FORCE)
    endif()

elseif (UNIX)
    option(SUPPORT_COCOA_FRONTEND "Build macOS native Cocoa app" OFF)
    option(SUPPORT_GCU_FRONTEND   "Build with GCU (curses/ncurses) support" ON)
    option(SUPPORT_X11_FRONTEND   "Build with X11 support" ON)

else()
    option(SUPPORT_COCOA_FRONTEND "Build macOS native Cocoa app" OFF)
    option(SUPPORT_GCU_FRONTEND   "Build with GCU (curses/ncurses) support" OFF)
    option(SUPPORT_X11_FRONTEND   "Build with X11 support" OFF)
endif()

# Print final values of the above options
if (APPLE)
    if (SUPPORT_COCOA_FRONTEND)
        message(STATUS "Building a macOS native Cocoa app")
    endif()
    message(STATUS "Building for macOS architectures: ${CMAKE_OSX_ARCHITECTURES}")
    message(STATUS "Minimum macOS deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
if (SUPPORT_GCU_FRONTEND)
    message(STATUS "GCU (curses/ncurses) graphics support enabled")
endif()
if (SUPPORT_X11_FRONTEND)
    message(STATUS "X11 graphics support enabled")
endif()

# =============================================================================
# Validate environment before project() is set
# =============================================================================

# macOS sanity checks
if (APPLE)

    # macOS native Cocoa app
    if (SUPPORT_COCOA_FRONTEND)
        # Cocoa builds require Clang
        if (DEFINED ENV{CC} AND NOT _cc_env_set_to_clang)
            message(FATAL_ERROR
                "Building a macOS native Cocoa app requires Clang (expected: 'clang', actual: '$ENV{CC}'). "
                "Use the default macOS compiler or build the console version with -DSUPPORT_COCOA_FRONTEND=OFF.")
        endif()
    else()
        # Prevent macOS universal builds with X11/curses
        # (Homebrew libs are single-arch)
        if (CMAKE_OSX_ARCHITECTURES)
            list(LENGTH CMAKE_OSX_ARCHITECTURES _arch_count)
            if (_arch_count GREATER 1)
                message(FATAL_ERROR
                    "Only a macOS native Cocoa app (-DSUPPORT_COCOA_FRONTEND=ON) supports universal builds. "
                    "But Homebrew X11/curses libraries are only available for the host architecture (${OS_ARCH}).")
            endif()
        endif()
    endif()

endif()

# =============================================================================
# Project settings
# =============================================================================

# Version information defined here is injected into `src/defines.h`.
project(Sil
    VERSION 1.5.1.0
    LANGUAGES C
    DESCRIPTION "Sil-Q is a free, single-player dungeon infiltration game")
set(PROJECT_VERSION_SUFFIX "-beta2")
set(PROJECT_VERSION_STRING "${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")
# Apple's CFBundleShortVersionString requires exactly three period-separated
# integers (major.minor.patch), so we provide a three-component version.
set(PROJECT_VERSION_SHORT "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

message(STATUS "Project version: ${PROJECT_VERSION} (${PROJECT_VERSION_STRING})")

# =============================================================================
# Project metadata
# =============================================================================

set(SIL_NAME "Sil")
set(SIL_EXECUTABLE "sil")
set(SIL_COPYRIGHT "half & Scatha")

# =============================================================================
# Host environment
# =============================================================================

# Detect host operating system
set(OS_NAME "unknown")
if (APPLE)
    set(OS_NAME "macos")
elseif (WIN32)
    set(OS_NAME "windows")
elseif (UNIX AND NOT APPLE)
    set(OS_NAME "unix")
endif()
message(STATUS "Host operating system: ${OS_NAME}")

# Detect host architecture
set(OS_ARCH "amd64")
string(REGEX MATCH "(arm64|aarch64)" IS_ARM "${CMAKE_SYSTEM_PROCESSOR}")
if (IS_ARM)
    set(OS_ARCH "arm64")
endif()
message(STATUS "Host architecture: ${OS_ARCH}")

# =============================================================================
# Source files
# =============================================================================

# Low-level utility modules
set(Z_SOURCES
    src/z-form.c
    src/z-rand.c
    src/z-term.c
    src/z-util.c
    src/z-virt.c
)

# Core game logic
set(SIL_SOURCES
    src/birth.c
    src/cave.c
    src/cmd1.c
    src/cmd2.c
    src/cmd3.c
    src/cmd4.c
    src/cmd5.c
    src/cmd6.c
    src/dungeon.c
    src/files.c
    src/generate.c
    src/init1.c
    src/init2.c
    src/load.c
    src/melee1.c
    src/melee2.c
    src/monster1.c
    src/monster2.c
    src/obj-info.c
    src/object1.c
    src/object2.c
    src/randart.c
    src/save.c
    src/spells1.c
    src/spells2.c
    src/squelch.c
    src/tables.c
    src/use-obj.c
    src/util.c
    src/variable.c
    src/wizard1.c
    src/wizard2.c
    src/xtra1.c
    src/xtra2.c
)

# Base sources used by all platforms
set(BASE_SOURCES ${SIL_SOURCES} ${Z_SOURCES})

# Platform-specific sources
set(PLATFORM_SOURCES "")

# macOS native Cocoa app
if (SUPPORT_COCOA_FRONTEND)
    list(APPEND PLATFORM_SOURCES src/main-cocoa.m)

# Microsoft Windows
elseif (WIN32)
    list(APPEND PLATFORM_SOURCES
        src/main-win.c
        src/readdib.c
        src/sil.rc
    )

# Linux and (non-Cocoa) macOS
else()
    list(APPEND PLATFORM_SOURCES src/main.c)

    if (SUPPORT_GCU_FRONTEND)
        list(APPEND PLATFORM_SOURCES src/main-gcu.c)
    endif()

    if (SUPPORT_X11_FRONTEND)
        list(APPEND PLATFORM_SOURCES
            src/maid-x11.c
            src/main-x11.c
        )
    endif()
endif()

# =============================================================================
# Target definition
# =============================================================================

# macOS native Cocoa app
if (SUPPORT_COCOA_FRONTEND)
    # Target named "Sil" to produce Sil.app
    set(SIL_TARGET "${SIL_NAME}")
    add_executable(${SIL_TARGET} MACOSX_BUNDLE ${BASE_SOURCES} ${PLATFORM_SOURCES})

# Microsoft Windows
elseif (WIN32)
    set(SIL_TARGET "${SIL_EXECUTABLE}")
    add_executable(${SIL_TARGET} WIN32 ${BASE_SOURCES} ${PLATFORM_SOURCES})

# Linux and (non-Cocoa) macOS
else()
    set(SIL_TARGET "${SIL_EXECUTABLE}")
    add_executable(${SIL_TARGET} ${BASE_SOURCES} ${PLATFORM_SOURCES})
endif()

target_include_directories(${SIL_TARGET} PRIVATE src)

# =============================================================================
# Compiler settings
# =============================================================================

# C standard: GNU99 for Linux and (non-Cocoa) macos, C99 for others
if (NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    message(STATUS "Setting C standard to C${CMAKE_C_STANDARD}")

    if (NOT WIN32 AND NOT SUPPORT_COCOA_FRONTEND)
        set(CMAKE_C_EXTENSIONS ON)
        message(STATUS "Enabling compiler-specific C extensions")
    else()
        set(CMAKE_C_EXTENSIONS OFF)
        message(STATUS "Disabling compiler-specific C extensions")
    endif()
endif()

# Common defines
target_compile_definitions(${SIL_TARGET} PRIVATE
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
    VERSION_EXTRA=${PROJECT_VERSION_TWEAK}
    VERSION_STRING="${PROJECT_VERSION_STRING}"
)

if (NOT WIN32)
    target_compile_definitions(${SIL_TARGET} PRIVATE HAVE_MKSTEMP)
endif()

# Compiler warnings used by all platforms
target_compile_options(${SIL_TARGET} PRIVATE -Wall -Wextra)

# Apple's Xcode Clang still defaults to -fcommon (unlike upstream LLVM Clang and
# GCC 10+). This causes linker warnings about reducing alignment of __DATA,__common.
# See: https://ffmpeg.org/pipermail/ffmpeg-cvslog/2025-May/148334.html
if (APPLE)
    target_compile_options(${SIL_TARGET} PRIVATE -fno-common)
endif()

# Extended compiler warnings for Linux and (non-Cocoa) macOS builds
if (NOT WIN32 AND NOT SUPPORT_COCOA_FRONTEND)
    target_compile_options(${SIL_TARGET} PRIVATE
        -Warray-bounds
        -Wbad-function-cast
        -Wcast-align
        -Wdouble-promotion
        -Wfloat-equal
        -Wimplicit-fallthrough
        -Winline
        -Wmissing-declarations
        -Wmissing-format-attribute
        -Wmissing-include-dirs
        -Wnested-externs
        -Wpointer-arith
        -Wredundant-decls
        -Wswitch-enum
        -Wundef
        -Wunreachable-code
    )
    # GCC-only compiler warnings (not supported by Clang)
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${SIL_TARGET} PRIVATE
            -Wold-style-declaration
            -Wold-style-definition
        )
    endif()
endif()

# Whether to treat compiler warnings as errors
option(BUILD_ENABLE_WERROR "Treat compiler warnings as errors" OFF)
if (BUILD_ENABLE_WERROR)
    target_compile_options(${SIL_TARGET} PRIVATE -Werror)
endif()

# Whether to enable ASan/UBSan sanitizers
option(BUILD_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if (BUILD_ENABLE_SANITIZERS)
    target_compile_options(${SIL_TARGET} PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(${SIL_TARGET} PRIVATE -fsanitize=address -fsanitize=undefined)
endif()

# =============================================================================
# Platform-specific configuration
# =============================================================================

# macOS native Cocoa app
if (SUPPORT_COCOA_FRONTEND)
    enable_language(OBJC)

    target_compile_definitions(${SIL_TARGET} PRIVATE
        MACH_O_CARBON
        COCOA
        USE_PRIVATE_SAVE_PATH
        PRIVATE_USER_PATH
        RUNTIME_PRIVATE_USER_PATH
    )

    # Objective-C flags for main-cocoa.m
    set_source_files_properties(src/main-cocoa.m PROPERTIES
        LANGUAGE OBJC
        COMPILE_FLAGS "-fobjc-arc"
    )

    # Link Cocoa framework
    target_link_libraries(${SIL_TARGET} PRIVATE "-framework Cocoa")

    # Check for autoconf.h
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/autoconf.h")
        target_compile_definitions(${SIL_TARGET} PRIVATE HAVE_CONFIG_H)
    endif()

    # ---- App bundle configuration ----

    # Configure Info.plist (escape & for XML)
    string(REPLACE "&" "&amp;" SIL_COPYRIGHT_XML "${SIL_COPYRIGHT}")
    configure_file(
        src/cocoa/Info.plist.in
        "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        @ONLY
    )
    set_target_properties(${SIL_TARGET} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    )

    # Helper variable for the Resources directory inside the bundle
    set(BUNDLE_RESOURCES_DIR "$<TARGET_FILE_DIR:${SIL_TARGET}>/../Resources")

    # Copy icon files
    foreach(ICON_FILE Sil_Icons.icns Save.icns Edit.icns Data.icns)
        add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/cocoa/${ICON_FILE}"
                "${BUNDLE_RESOURCES_DIR}/${ICON_FILE}"
        )
    endforeach()

    # Copy MainMenu.nib
    add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${BUNDLE_RESOURCES_DIR}/en.lproj"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/src/cocoa/en.lproj/MainMenu.nib"
            "${BUNDLE_RESOURCES_DIR}/en.lproj/MainMenu.nib"
    )

    # Create game data directories inside the bundle
    add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_RESOURCES_DIR}/lib/docs"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_RESOURCES_DIR}/lib/edit"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_RESOURCES_DIR}/lib/pref"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_RESOURCES_DIR}/lib/xtra"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_RESOURCES_DIR}/lib/xtra/graf"
    )

    # Copy game data files into the bundle
    file(GLOB SIL_DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/lib/docs/*.txt")
    file(GLOB SIL_EDIT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/lib/edit/*.txt")
    file(GLOB SIL_PREF_FILES "${CMAKE_CURRENT_SOURCE_DIR}/lib/pref/*.prf")

    foreach(F ${SIL_DOC_FILES})
        add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${F}"
                "${BUNDLE_RESOURCES_DIR}/lib/docs/"
        )
    endforeach()
    foreach(F ${SIL_EDIT_FILES})
        add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${F}"
                "${BUNDLE_RESOURCES_DIR}/lib/edit/"
        )
    endforeach()
    foreach(F ${SIL_PREF_FILES})
        add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${F}"
                "${BUNDLE_RESOURCES_DIR}/lib/pref/"
        )
    endforeach()

    # Tutorial file
    add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/xtra/tutorial"
            "${BUNDLE_RESOURCES_DIR}/lib/xtra/tutorial"
    )

    # Tileset
    add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/xtra/graf/16x16_microchasm.png"
            "${BUNDLE_RESOURCES_DIR}/lib/xtra/graf/16x16_microchasm.png"
    )

    # Rename executable from "Sil" to "sil" (macOS bundle uses target name for both)
    add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rename
            "$<TARGET_FILE:${SIL_TARGET}>"
            "$<TARGET_BUNDLE_CONTENT_DIR:${SIL_TARGET}>/MacOS/${SIL_EXECUTABLE}"
    )

    # Post-build steps
    add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
        # Set bundle bit
        COMMAND SetFile -a B "$<TARGET_BUNDLE_DIR:${SIL_TARGET}>"
        # Remove extended attributes to prevent triggering macOS Gatekeeper
        # when launching Sil.app. This works only when the user ran the build
        # locally on their computer. It does not prevent triggering macOS
        # Gatekeeper when the user downloads a pre-generated release artifact.
        COMMAND xattr -rc "$<TARGET_BUNDLE_DIR:${SIL_TARGET}>"
        # Clean metadata
        COMMAND dot_clean -m "$<TARGET_BUNDLE_DIR:${SIL_TARGET}>"
        # Codesign
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:${SIL_TARGET}>"
        COMMENT "Signing ${SIL_NAME}.app"
    )

# Microsoft Windows (MinGW/MSYS2)
elseif (WIN32)
    target_compile_definitions(${SIL_TARGET} PRIVATE WINDOWS)
    target_link_libraries(${SIL_TARGET} PRIVATE winmm msimg32)
    target_link_options(${SIL_TARGET} PRIVATE -mwindows $<$<CONFIG:Release>:-s>)

    # Special define for readdib.c (handles legacy Windows NEAR keyword)
    set_source_files_properties(src/readdib.c PROPERTIES
        COMPILE_DEFINITIONS "NEAR="
    )

# Linux and (non-Cocoa) macOS
else()
    if (SUPPORT_GCU_FRONTEND)
        find_package(Curses REQUIRED)
        target_compile_definitions(${SIL_TARGET} PRIVATE USE_GCU)
        target_link_libraries(${SIL_TARGET} PRIVATE ${CURSES_LIBRARIES})
        target_include_directories(${SIL_TARGET} PRIVATE ${CURSES_INCLUDE_DIRS})
    endif()

    if (SUPPORT_X11_FRONTEND)
        find_package(X11 REQUIRED)
        target_compile_definitions(${SIL_TARGET} PRIVATE USE_X11)
        target_link_libraries(${SIL_TARGET} PRIVATE ${X11_LIBRARIES})
        target_include_directories(${SIL_TARGET} PRIVATE ${X11_INCLUDE_DIR})
    endif()
endif()

# =============================================================================
# Install
# =============================================================================

if (SUPPORT_COCOA_FRONTEND)
    install(TARGETS ${SIL_TARGET} BUNDLE DESTINATION .)
else()
    install(TARGETS ${SIL_TARGET} RUNTIME DESTINATION .)
endif()
