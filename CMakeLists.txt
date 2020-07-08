CMAKE_MINIMUM_REQUIRED(VERSION 3.1 FATAL_ERROR)
PROJECT(SilQ VERSION 1.4.2 LANGUAGES C)

IF (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    MESSAGE(FATAL_ERROR "Not support to CMake in Windows")
ENDIF()

OPTION(SUPPORT_NCURSES_FRONTEND "Support to NCurses Frontend." OFF)
OPTION(SUPPORT_SDL_FRONTEND "Support to SDL Frontend." OFF)
OPTION(SUPPORT_X11_FRONTEND "Support to X11 Frontend." ON)
OPTION(SUPPORT_SDL_SOUND "Support to Sound System with SDL." OFF)

# For default, X11 is the frontend, if the user want specific
# a new frontend is needed set to false (OFF) the frontend X11
IF (SUPPORT_NCURSES_FRONTEND OR SUPPORT_SDL_FRONTEND)
    SET(SUPPORT_X11_FRONTEND OFF)
ENDIF()

ADD_EXECUTABLE(SilQ
        src/automaton.c
        src/birth.c
        src/cave.c
        src/cmd1.c
        src/cmd2.c
        src/cmd3.c
        src/cmd4.c
        src/cmd5.c
        src/cmd6.c
        src/dungeon.c
        src/init1.c
        src/init2.c
        src/load.c
        src/files.c
        src/generate.c
        src/maid-x11.c
#        src/main-cap.c
#        src/main-cocoa-NMcC.m
#        src/main-cocoa.m
#        src/main-crb.c
#        src/main-gcu.c
#        src/main-gtk.c
#        src/main-nfe.c
#        src/main-win.c
        $<$<BOOL:${SUPPORT_X11_FRONTEND}>:src/main-x11.c>
#        src/main-xxx.c
        src/main.c
        src/melee1.c
        src/melee2.c
        src/monster1.c
        src/monster2.c
        src/obj-info.c
        src/object1.c
        src/object2.c
        src/randart.c
#        src/readdib.c
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
        src/z-virt.c)

SET_TARGET_PROPERTIES(SilQ PROPERTIES C_STANDARD 11)

IF (SUPPORT_X11_FRONTEND)

    INCLUDE(src/cmake/macros/X11_Frontend.cmake)
    CONFIGURE_X11_FRONTEND(SilQ)

ELSEIF(SUPPORT_NCURSES_FRONTEND)

    INCLUDE(src/cmake/macros/CURSES_Frontend.cmake)
    CONFIGURE_CURSES_FRONTEND(SilQ)

ELSEIF(SUPPORT_SDL_FRONTEND)

    INCLUDE(src/cmake/macros/SDL_Frontend.cmake)
    CONFIGURE_SDL_FRONTEND(SilQ)

ELSEIF(SUPPORT_SDL_SOUND)

    INCLUDE(src/cmake/macros/SDL_Sound.cmake)
    CONFIGURE_SDL_SOUND(SilQ)

ENDIF()

#[[
For Linux Systems the compilation without the libm library
throw undefined references to mathematical functions, this requirement
is only present in Linux Systems.

Wikipedia say: Under Linux and BSD, the mathematical functions
(as declared in <math.h>) are bundled separately in the mathematical
library libm. Therefore, if any of those functions are used, the linker
must be given the directive -lm.

Ref: https://en.wikipedia.org/wiki/C_mathematical_functions#libm
]]
TARGET_LINK_LIBRARIES(SilQ PRIVATE $<$<PLATFORM_ID:Linux>:m>)

MESSAGE(STATUS "Moving files needed")

FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/lib DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
