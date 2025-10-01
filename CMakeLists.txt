cmake_minimum_required(VERSION 3.15)
project(sil-more)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LOG_MODE_TIMESTAMP "Create new log with timestamp each time" OFF)

if (LOG_MODE_TIMESTAMP)
  target_compile_definitions(sil-more PUBLIC LOG_MODE_TIMESTAMP)
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
    src/main-gcu.c  # We're GCU-only for a moment
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
    # src/readdib.c  # No Windows yet
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
    src/z-form.c
    src/z-rand.c
    src/z-term.c
    src/z-util.c
    src/z-virt.c
)

target_compile_options(sil-more PUBLIC -Wall -Wextra -std=c11)
target_compile_definitions(sil-more PRIVATE -DUSE_GCU)
target_include_directories(sil-more PRIVATE src)
target_link_libraries(sil-more PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image Curses::Curses log)
