cmake_minimum_required(VERSION 3.18)
project(sil-more C)

# Project language standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LOG_MODE_TIMESTAMP "Create new log with timestamp each time" OFF)
option(STEAMDECK_SUPPORT "Build for Steam Deck" OFF)
option(SIL_USE_LOCAL_DATA "Store generated data inside the game directory (debug builds)" OFF)
option(SIL_BUILD_WITH_SDL_SOURCES "Build SDL dependencies from source under external/ (useful for Android)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(ANDROID)
  # Ensure Android logging symbols are resolvable for SDL and dependent shared libs.
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -llog")
endif()

if (SIL_BUILD_WITH_SDL_SOURCES)
  set(SIL_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL" "${CMAKE_BINARY_DIR}/_deps/SDL")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_image/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_image" "${CMAKE_BINARY_DIR}/_deps/SDL_image")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL_image is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_ttf/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_ttf" "${CMAKE_BINARY_DIR}/_deps/SDL_ttf")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL_ttf is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (ANDROID AND TARGET SDL3-shared)
    # Needed for __android_log_print / __android_log_write in SDL Android backend.
    target_link_libraries(SDL3-shared PRIVATE log)
  endif()
else()
  find_package(SDL3 REQUIRED)
  find_package(SDL3_ttf REQUIRED)
  find_package(SDL3_image REQUIRED)
endif()

add_subdirectory(src/log)

set(SIL_MORE_SOURCES
    src/blitz.c
    src/birth.c
    src/cave.c
    src/cJSON.c
    src/cmd1.c
    src/cmd2.c
    src/cmd3.c
    src/cmd4.c
    src/cmd5.c
    src/cmd6.c
    src/dungeon.c
    src/files.c
    src/format.c
    src/generate.c
    src/gen-log.c
    src/init1.c
    src/init2.c
    src/item_set.c
    src/load.c
    src/main.c
    src/main-sdl.c
    src/melee1.c
    src/melee2.c
    src/metarun.c
    src/metarun_legacy.c
    src/quest.c
    src/monster1.c
    src/monster2.c
    src/obj-info.c
    src/object1.c
    src/object2.c
    src/drop_system.c
    src/thrall_quest.c
    src/player/killer.c
    src/pane.c
    src/randart.c
    src/rng.c
    src/save.c
    src/score/score_io.c
    src/score/score_logic.c
    src/score/score_guid.c
    src/score/score_artefact.c
    src/score/score_runs.c
    src/score/score_ui.c
    src/sdl-config.c
    src/sdl-sound.c
    src/sound-config.c
    src/fs/io_sdl.c
    src/fs/path.c
    src/ui/colors.c
    src/ui/story_font.c
    src/log/bootstrap.c
    src/log/fatal.c
    src/spells1.c
    src/spells2.c
    src/squelch.c
    src/supplies.c
    src/tables.c
    src/use-obj.c
    src/util.c
    src/variable.c
    src/wizard1.c
    src/wizard2.c
    src/xtra1.c
    src/xtra2.c
    src/z-term.c
    src/support/strl.c
)

if(ANDROID)
  add_library(sil-more SHARED ${SIL_MORE_SOURCES})
else()
  add_executable(sil-more ${SIL_MORE_SOURCES})
endif()

if(ANDROID)
  # SDL's Android launcher expects libmain.so by convention.
  set_target_properties(sil-more PROPERTIES OUTPUT_NAME "main")
endif()

# Include Windows resource (.rc) so the executable has an icon on Windows
if(WIN32 AND NOT ANDROID)
  # Prefer the .rc in src/icons which references the Sil.ico file present there.
  target_sources(sil-more PRIVATE ${CMAKE_SOURCE_DIR}/src/icons/sil.rc)
endif()

# add_library(log STATIC src/log/log.c)
target_include_directories(log PUBLIC src/log)

# add_executable(sil-more ${SIL_MORE_SOURCES})

target_compile_options(sil-more PRIVATE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
  $<$<C_COMPILER_ID:MSVC>:/W4>
)

target_include_directories(sil-more PRIVATE src)
if(MINGW)
  target_link_options(sil-more PRIVATE -mwindows)
endif()

target_link_libraries(sil-more PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image log)

# Add compile definitions after target is created
if (LOG_MODE_TIMESTAMP)
  target_compile_definitions(sil-more PUBLIC LOG_MODE_TIMESTAMP)
endif()

if (STEAMDECK_SUPPORT)
    target_compile_definitions(sil-more PUBLIC STEAMDECK_SUPPORT)
endif()

if (SIL_USE_LOCAL_DATA)
    target_compile_definitions(sil-more PUBLIC SIL_USE_LOCAL_DATA)
endif()
