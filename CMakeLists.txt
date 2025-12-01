cmake_minimum_required(VERSION 3.10)
project(sil-more C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LOG_MODE_TIMESTAMP "Create new log with timestamp each time" OFF)
option(STEAMDECK_SUPPORT "Build for Steam Deck" OFF)
option(SIL_USE_LOCAL_DATA "Store generated data inside the game directory (debug builds)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release")
endif()

find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(SDL3_image REQUIRED)

add_subdirectory(src/log)

add_executable(sil-more
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

# Include Windows resource (.rc) so the executable has an icon on Windows
if(WIN32)
  # Prefer the .rc in src/icons which references the Sil.ico file present there.
  target_sources(sil-more PRIVATE ${CMAKE_SOURCE_DIR}/src/icons/sil.rc)
endif()

# add_library(log STATIC src/log/log.c)
target_include_directories(log PUBLIC src/log)

# add_executable(sil-more ${SIL_MORE_SOURCES})

target_compile_options(sil-more PUBLIC -Wall -Wextra -std=c11)
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
