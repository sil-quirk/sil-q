cmake_minimum_required(VERSION 3.10)
project(sil-more C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_SDL "Build with SDL support" ON)
option(USE_GCU "Build with GCU (ncurses) support" OFF)

option(LOG_MODE_TIMESTAMP "Create new log with timestamp each time" OFF)
option(STEAMDECK_SUPPORT "Build for Steam Deck" OFF)

if (LOG_MODE_TIMESTAMP)
  target_compile_definitions(sil-more PUBLIC LOG_MODE_TIMESTAMP)
endif()

if (STEAMDECK_SUPPORT)
    target_compile_definitions(sil-more PUBLIC STEAMDECK_SUPPORT)
endif()

find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(Curses)
if(Curses_FOUND AND NOT TARGET Curses::Curses)
  add_library(Curses::Curses INTERFACE IMPORTED)
  set_target_properties(
    Curses::Curses
    PROPERTIES
      INTERFACE_LINK_LIBRARIES "${CURSES_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${CURSES_INCLUDE_DIRS}"
  )
endif()

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
    src/dump_items.c
    src/dungeon.c
    src/files.c
    src/generate.c
    src/init1.c
    src/init2.c
    src/load.c
    src/main.c
    src/main-sdl.c
    src/melee1.c
    src/melee2.c
    src/metarun.c
    src/monster1.c
    src/monster2.c
    src/obj-info.c
    src/object1.c
    src/object2.c
    src/pane.c
    src/randart.c
    src/save.c
    src/sdl-config.c
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
    src/z-form.c
    src/z-rand.c
    src/z-term.c
    src/z-util.c
    src/z-virt.c
)

# Include Windows resource (.rc) so the executable has an icon on Windows
if(WIN32)
  # Prefer the .rc in src/icons which references the Sil.ico file present there.
  target_sources(sil-more PRIVATE ${CMAKE_SOURCE_DIR}/src/icons/sil.rc)
endif()

# add_library(log STATIC src/log/log.c)
target_include_directories(log PUBLIC src/log)

if(USE_SDL)
  list(APPEND SIL_MORE_SOURCES src/main-sdl.c)
endif()

# add_executable(sil-more ${SIL_MORE_SOURCES})

target_compile_options(sil-more PUBLIC -Wall -Wextra -std=c11)
target_include_directories(sil-more PRIVATE src)

if(MINGW)
  target_link_options(sil-more PRIVATE -mwindows)
endif()

if(USE_SDL)
  find_package(SDL3 REQUIRED CONFIG)
  find_package(SDL3_ttf REQUIRED CONFIG)
  find_package(SDL3_image REQUIRED CONFIG)
  target_compile_definitions(sil-more PRIVATE USE_SDL)
  target_link_libraries(sil-more PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image log)
endif()

if(USE_GCU)
  find_package(Curses REQUIRED)
  target_compile_definitions(sil-more PRIVATE USE_GCU)
  target_link_libraries(sil-more PRIVATE Curses::Curses log)
endif()
