CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT(SilQ VERSION 1.5.0 LANGUAGES C)

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
        src/main-x11.c
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
TARGET_COMPILE_DEFINITIONS(SilQ PRIVATE -DUSE_X11)
TARGET_LINK_LIBRARIES(SilQ PRIVATE X11)
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/lib DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
